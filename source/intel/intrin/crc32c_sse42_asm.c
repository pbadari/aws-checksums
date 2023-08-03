/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#include <aws/checksums/private/crc_priv.h>

#include <aws/common/cpuid.h>
#include <emmintrin.h>
#include <smmintrin.h>
#include <wmmintrin.h>
#include <immintrin.h>

#define zalign(x) __attribute__((aligned((x))))

/* clang-format off */

/* this implementation is only for the x86_64 intel architecture */
#if defined(__x86_64__)
#    if defined(__clang__)
#        pragma clang diagnostic push
#        pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"
#    endif

/* use local labels, so that linker doesn't think these are functions it can deadstrip */
#    ifdef __APPLE__
#        define LABEL(label) "L_" #label "_%="
#    else
#        define LABEL(label) ".L_" #label "_%="
#    endif

/*
 * Factored out common inline asm for folding crc0,crc1,crc2 stripes in rcx, r11, r10 using
 * the specified Magic Constants K1 and K2.
 * Assumes rcx, r11, r10 contain crc0, crc1, crc2 that need folding
 * Utilizes xmm1, xmm2, xmm3, xmm4 as well as clobbering r8, r9, r11
 * Result is placed in ecx
 */
#    define FOLD_K1K2(K1, K2)                                                                                          \
        "movl             " #K1 ", %%r8d    # Magic K1 constant \n"                                                    \
        "movl             " #K2 ", %%r9d    # Magic K2 constant \n"                                                    \
        "movq              %%rcx, %%xmm1   # crc0 into lower dword of xmm1 \n"                                         \
        "movq               %%r8, %%xmm3   # K1 into lower dword of xmm3 \n"                                           \
        "movq              %%r11, %%xmm2   # crc1 into lower dword of xmm2 \n"                                         \
        "movq               %%r9, %%xmm4   # K2 into lower dword of xmm4 \n"                                           \
        "pclmulqdq $0x00, %%xmm3, %%xmm1   # Multiply crc0 by K1 \n"                                                   \
        "pclmulqdq $0x00, %%xmm4, %%xmm2   # Multiply crc1 by K2 \n"                                                   \
        "xor               %%rcx, %%rcx    # \n"                                                                       \
        "xor               %%r11, %%r11    # \n"                                                                       \
        "movq             %%xmm1, %%r8     # \n"                                                                       \
        "movq             %%xmm2, %%r9     # \n"                                                                       \
        "crc32q             %%r8, %%rcx    # folding crc0 \n"                                                          \
        "crc32q             %%r9, %%r11    # folding crc1 \n"                                                          \
        "xor              %%r10d, %%ecx    # combine crc2 and crc0 \n"                                                 \
        "xor              %%r11d, %%ecx    # combine crc1 and crc0 \n"

/**
 * Private (static) function.
 * Computes the Castagnoli CRC32c (iSCSI) of the specified data buffer using the Intel CRC32Q (quad word) machine
 * instruction by operating on 24-byte stripes in parallel. The results are folded together using CLMUL. This function
 * is optimized for exactly 256 byte blocks that are best aligned on 8-byte memory addresses. It MUST be passed a
 * pointer to input data that is exactly 256 bytes in length. Note: this function does NOT invert bits of the input crc
 * or return value.
 */
static inline uint32_t s_crc32c_sse42_clmul_256(const uint8_t *input, uint32_t crc) {
    __asm__ __volatile__(
        "xor          %%r11, %%r11    # zero all 64 bits in r11, will track crc1 \n"
        "xor          %%r10, %%r10    # zero all 64 bits in r10, will track crc2 \n"

        "crc32q    0(%[in]), %%rcx    # crc0 \n"
        "crc32q   88(%[in]), %%r11    # crc1 \n"
        "crc32q  176(%[in]), %%r10    # crc2 \n"

        "crc32q    8(%[in]), %%rcx    # crc0 \n"
        "crc32q   96(%[in]), %%r11    # crc1 \n"
        "crc32q  184(%[in]), %%r10    # crc2 \n"

        "crc32q   16(%[in]), %%rcx    # crc0 \n"
        "crc32q  104(%[in]), %%r11    # crc1 \n"
        "crc32q  192(%[in]), %%r10    # crc2 \n"

        "crc32q   24(%[in]), %%rcx    # crc0 \n"
        "crc32q  112(%[in]), %%r11    # crc1 \n"
        "crc32q  200(%[in]), %%r10    # crc2 \n"

        "crc32q   32(%[in]), %%rcx    # crc0 \n"
        "crc32q  120(%[in]), %%r11    # crc1 \n"
        "crc32q  208(%[in]), %%r10    # crc2 \n"

        "crc32q   40(%[in]), %%rcx    # crc0 \n"
        "crc32q  128(%[in]), %%r11    # crc1 \n"
        "crc32q  216(%[in]), %%r10    # crc2 \n"

        "crc32q   48(%[in]), %%rcx    # crc0 \n"
        "crc32q  136(%[in]), %%r11    # crc1 \n"
        "crc32q  224(%[in]), %%r10    # crc2 \n"

        "crc32q   56(%[in]), %%rcx    # crc0 \n"
        "crc32q  144(%[in]), %%r11    # crc1 \n"
        "crc32q  232(%[in]), %%r10    # crc2 \n"

        "crc32q   64(%[in]), %%rcx    # crc0 \n"
        "crc32q  152(%[in]), %%r11    # crc1 \n"
        "crc32q  240(%[in]), %%r10    # crc2 \n"

        "crc32q   72(%[in]), %%rcx    # crc0 \n"
        "crc32q  160(%[in]), %%r11    # crc1 \n"
        "crc32q  248(%[in]), %%r10    # crc2 \n"

        "crc32q   80(%[in]), %%rcx    # crc0 \n"
        "crc32q  168(%[in]), %%r11    # crc2 \n"

        FOLD_K1K2($0x1b3d8f29, $0x39d3b296) /* Magic Constants used to fold crc stripes into ecx */

        /* output registers
         [crc] is an input and and output so it is marked read/write (i.e. "+c")*/
        : [ crc ] "+c"(crc)
        /* input registers */
        : [ in ] "d"(input)

        /* additional clobbered registers */
        : "%r8", "%r9", "%r11", "%r10", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "cc");
    return crc;
}

/**
 * Private (static) function.
 * Computes the Castagnoli CRC32c (iSCSI) of the specified data buffer using the Intel CRC32Q (quad word) machine
 * instruction by operating on 3 24-byte stripes in parallel. The results are folded together using CLMUL. This function
 * is optimized for exactly 1024 byte blocks that are best aligned on 8-byte memory addresses. It MUST be passed a
 * pointer to input data that is exactly 1024 bytes in length. Note: this function does NOT invert bits of the input crc
 * or return value.
 */
static inline uint32_t s_crc32c_sse42_clmul_1024(const uint8_t *input, uint32_t crc) {
    __asm__ __volatile__(
        "xor          %%r11, %%r11    # zero all 64 bits in r11, will track crc1 \n"
        "xor          %%r10, %%r10    # zero all 64 bits in r10, will track crc2 \n"

        "movl            $5, %%r8d    # Loop 5 times through 64 byte chunks in 3 parallel stripes \n"

        LABEL(loop_1024) ": \n"

        "prefetcht0  128(%[in])       # \n"
        "prefetcht0  472(%[in])       # \n"
        "prefetcht0  808(%[in])       # \n"

        "crc32q    0(%[in]), %%rcx    # crc0: stripe0 \n"
        "crc32q  344(%[in]), %%r11    # crc1: stripe1 \n"
        "crc32q  680(%[in]), %%r10    # crc2: stripe2 \n"

        "crc32q    8(%[in]), %%rcx    # crc0 \n"
        "crc32q  352(%[in]), %%r11    # crc1 \n"
        "crc32q  688(%[in]), %%r10    # crc2 \n"

        "crc32q   16(%[in]), %%rcx    # crc0 \n"
        "crc32q  360(%[in]), %%r11    # crc1 \n"
        "crc32q  696(%[in]), %%r10    # crc2 \n"

        "crc32q   24(%[in]), %%rcx    # crc0 \n"
        "crc32q  368(%[in]), %%r11    # crc1 \n"
        "crc32q  704(%[in]), %%r10    # crc2 \n"

        "crc32q   32(%[in]), %%rcx    # crc0 \n"
        "crc32q  376(%[in]), %%r11    # crc1 \n"
        "crc32q  712(%[in]), %%r10    # crc2 \n"

        "crc32q   40(%[in]), %%rcx    # crc0 \n"
        "crc32q  384(%[in]), %%r11    # crc1 \n"
        "crc32q  720(%[in]), %%r10    # crc2 \n"

        "crc32q   48(%[in]), %%rcx    # crc0 \n"
        "crc32q  392(%[in]), %%r11    # crc1 \n"
        "crc32q  728(%[in]), %%r10    # crc2 \n"

        "crc32q   56(%[in]), %%rcx    # crc0 \n"
        "crc32q  400(%[in]), %%r11    # crc1 \n"
        "crc32q  736(%[in]), %%r10    # crc2 \n"

        "add            $64, %[in]    # \n"
        "sub             $1, %%r8d    # \n"
        "jnz " LABEL(loop_1024) "     # \n"

        "crc32q    0(%[in]), %%rcx    # crc0 \n"
        "crc32q  344(%[in]), %%r11    # crc1 \n"
        "crc32q  680(%[in]), %%r10    # crc2 \n"

        "crc32q    8(%[in]), %%rcx    # crc0 \n"
        "crc32q  352(%[in]), %%r11    # crc1 \n"
        "crc32q  688(%[in]), %%r10    # crc2 \n"

        "crc32q   16(%[in]), %%rcx    # crc0 \n"
        "crc32q  696(%[in]), %%r10    # crc2 \n"

        FOLD_K1K2($0xe417f38a, $0x8f158014) /* Magic Constants used to fold crc stripes into ecx

                            output registers
                            [crc] is an input and and output so it is marked read/write (i.e. "+c")
                            we clobber the register for [input] (via add instruction) so we must also
                            tag it read/write (i.e. "+d") in the list of outputs to tell gcc about the clobber */
        : [ crc ] "+c"(crc), [ in ] "+d"(input)
        :
        /* additional clobbered registers */
        /* "cc" is the flags - we add and sub, so the flags are also clobbered */
        : "%r8", "%r9", "%r11", "%r10", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "cc");
    return crc;
}

/**
 * Private (static) function.
 * Computes the Castagnoli CRC32c (iSCSI) of the specified data buffer using the Intel CRC32Q (quad word) machine
 * instruction by operating on 24-byte stripes in parallel. The results are folded together using CLMUL. This function
 * is optimized for exactly 3072 byte blocks that are best aligned on 8-byte memory addresses. It MUST be passed a
 * pointer to input data that is exactly 3072 bytes in length. Note: this function does NOT invert bits of the input crc
 * or return value.
 */
static inline uint32_t s_crc32c_sse42_clmul_3072(const uint8_t *input, uint32_t crc) {
    __asm__ __volatile__(
        "xor          %%r11, %%r11    # zero all 64 bits in r11, will track crc1 \n"
        "xor          %%r10, %%r10    # zero all 64 bits in r10, will track crc2 \n"

        "movl           $16, %%r8d    # Loop 16 times through 64 byte chunks in 3 parallel stripes \n"

        LABEL(loop_3072) ": \n"

        "prefetcht0  128(%[in])       # \n"
        "prefetcht0 1152(%[in])       # \n"
        "prefetcht0 2176(%[in])       # \n"

        "crc32q    0(%[in]), %%rcx    # crc0: stripe0 \n"
        "crc32q 1024(%[in]), %%r11    # crc1: stripe1 \n"
        "crc32q 2048(%[in]), %%r10    # crc2: stripe2 \n"

        "crc32q    8(%[in]), %%rcx    # crc0: stripe0 \n"
        "crc32q 1032(%[in]), %%r11    # crc1: stripe1 \n"
        "crc32q 2056(%[in]), %%r10    # crc2: stripe2 \n"

        "crc32q   16(%[in]), %%rcx    # crc0: stripe0 \n"
        "crc32q 1040(%[in]), %%r11    # crc1: stripe1 \n"
        "crc32q 2064(%[in]), %%r10    # crc2: stripe2 \n"

        "crc32q   24(%[in]), %%rcx    # crc0: stripe0 \n"
        "crc32q 1048(%[in]), %%r11    # crc1: stripe1 \n"
        "crc32q 2072(%[in]), %%r10    # crc2: stripe2 \n"

        "crc32q   32(%[in]), %%rcx    # crc0: stripe0 \n"
        "crc32q 1056(%[in]), %%r11    # crc1: stripe1 \n"
        "crc32q 2080(%[in]), %%r10    # crc2: stripe2 \n"

        "crc32q   40(%[in]), %%rcx    # crc0: stripe0 \n"
        "crc32q 1064(%[in]), %%r11    # crc1: stripe1 \n"
        "crc32q 2088(%[in]), %%r10    # crc2: stripe2 \n"

        "crc32q   48(%[in]), %%rcx    # crc0: stripe0 \n"
        "crc32q 1072(%[in]), %%r11    # crc1: stripe1 \n"
        "crc32q 2096(%[in]), %%r10    # crc2: stripe2 \n"

        "crc32q   56(%[in]), %%rcx    # crc0: stripe0 \n"
        "crc32q 1080(%[in]), %%r11    # crc1: stripe1 \n"
        "crc32q 2104(%[in]), %%r10    # crc2: stripe2 \n"

        "add            $64, %[in]    # \n"
        "sub             $1, %%r8d    # \n"
        "jnz " LABEL(loop_3072) "     # \n"

        FOLD_K1K2(
            $0xa51b6135,
            $0x170076fa) /* Magic Constants used to fold crc stripes into ecx

                            output registers
                            [crc] is an input and and output so it is marked read/write (i.e. "+c")
                            we clobber the register for [input] (via add instruction) so we must also
                            tag it read/write (i.e. "+d") in the list of outputs to tell gcc about the clobber*/
        : [ crc ] "+c"(crc), [ in ] "+d"(input)
        :
        /* additional clobbered registers
          "cc" is the flags - we add and sub, so the flags are also clobbered */
        : "%r8", "%r9", "%r11", "%r10", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "cc");

    return crc;
}

/*
 * crc32c_avx512(): compute the crc32c of the buffer, where the buffer
 * length must be at least 256, and a multiple of 64. Based on:
 *
 * "Fast CRC Computation for Generic Polynomials Using PCLMULQDQ Instruction"
 *  V. Gopal, E. Ozturk, et al., 2009, http://intel.ly/2ySEwL0
 */
static uint32_t crc32c_avx512(const uint8_t *input, int length, uint32_t crc)
{
    /*
     * Definitions of the bit-reflected domain constants k1,k2,k3,k4,k5,k6
     * are similar to those given at the end of the paper
     *
     * k1 = ( x ^ ( 512 * 4 + 32 ) mod P(x) << 32 )' << 1
     * k2 = ( x ^ ( 512 * 4 - 32 ) mod P(x) << 32 )' << 1
     * k3 = ( x ^ ( 512 + 32 ) mod P(x) << 32 )' << 1
     * k4 = ( x ^ ( 512 - 32 ) mod P(x) << 32 )' << 1
     * k5 = ( x ^ ( 128 + 32 ) mod P(x) << 32 )' << 1
     * k6 = ( x ^ ( 128 - 32 ) mod P(x) << 32 )' << 1
     */

    static const uint64_t zalign(64) k1k2[] = { 0xdcb17aa4, 0xb9e02b86,
                                                0xdcb17aa4, 0xb9e02b86,
                                                0xdcb17aa4, 0xb9e02b86,
                                                0xdcb17aa4, 0xb9e02b86 };
    static const uint64_t zalign(64) k3k4[] = { 0x740eef02, 0x9e4addf8,
                                                0x740eef02, 0x9e4addf8,
                                                0x740eef02, 0x9e4addf8,
                                                0x740eef02, 0x9e4addf8 };
    static const uint64_t zalign(16) k5k6[] = { 0xf20c0dfe, 0x14cd00bd6 };
    static const uint64_t zalign(16) k7k8[] = { 0xdd45aab8, 0x000000000 };
    static const uint64_t zalign(16) poly[] = { 0x105ec76f1, 0xdea713f1 };

    __m512i x0, x1, x2, x3, x4, x5, x6, x7, x8, y5, y6, y7, y8;
    __m128i a0, a1, a2, a3;

    /*
     * There's at least one block of 256.
     */
    x1 = _mm512_loadu_si512((__m512i *)(input + 0x00));
    x2 = _mm512_loadu_si512((__m512i *)(input + 0x40));
    x3 = _mm512_loadu_si512((__m512i *)(input + 0x80));
    x4 = _mm512_loadu_si512((__m512i *)(input + 0xC0));

    x1 = _mm512_xor_si512(x1, _mm512_castsi128_si512(_mm_cvtsi32_si128(crc)));

    x0 = _mm512_load_si512((__m512i *)k1k2);

    input += 256;
    length -= 256;

    /*
     * Parallel fold blocks of 256, if any.
     */
    while (length >= 256)
    {
        x5 = _mm512_clmulepi64_epi128(x1, x0, 0x00);
        x6 = _mm512_clmulepi64_epi128(x2, x0, 0x00);
        x7 = _mm512_clmulepi64_epi128(x3, x0, 0x00);
        x8 = _mm512_clmulepi64_epi128(x4, x0, 0x00);


        x1 = _mm512_clmulepi64_epi128(x1, x0, 0x11);
        x2 = _mm512_clmulepi64_epi128(x2, x0, 0x11);
        x3 = _mm512_clmulepi64_epi128(x3, x0, 0x11);
        x4 = _mm512_clmulepi64_epi128(x4, x0, 0x11);

        y5 = _mm512_loadu_si512((__m512i *)(input + 0x00));
        y6 = _mm512_loadu_si512((__m512i *)(input + 0x40));
        y7 = _mm512_loadu_si512((__m512i *)(input + 0x80));
        y8 = _mm512_loadu_si512((__m512i *)(input + 0xC0));

        x1 = _mm512_ternarylogic_epi64(x1, x5, y5, 0x96);
        x2 = _mm512_ternarylogic_epi64(x2, x6, y6, 0x96);
        x3 = _mm512_ternarylogic_epi64(x3, x7, y7, 0x96);
        x4 = _mm512_ternarylogic_epi64(x4, x8, y8, 0x96);

        input += 256;
        length -= 256;
    }

    /*
     * Fold into 512-bits.
     */
    x0 = _mm512_load_si512((__m512i *)k3k4);

    x5 = _mm512_clmulepi64_epi128(x1, x0, 0x00);
    x1 = _mm512_clmulepi64_epi128(x1, x0, 0x11);
    x1 = _mm512_ternarylogic_epi64(x1, x2, x5, 0x96);

    x5 = _mm512_clmulepi64_epi128(x1, x0, 0x00);
    x1 = _mm512_clmulepi64_epi128(x1, x0, 0x11);
    x1 = _mm512_ternarylogic_epi64(x1, x3, x5, 0x96);

    x5 = _mm512_clmulepi64_epi128(x1, x0, 0x00);
    x1 = _mm512_clmulepi64_epi128(x1, x0, 0x11);
    x1 = _mm512_ternarylogic_epi64(x1, x4, x5, 0x96);

    /*
     * Single fold blocks of 64, if any.
     */
    while (length >= 64)
    {
        x2 = _mm512_loadu_si512((__m512i *)input);

        x5 = _mm512_clmulepi64_epi128(x1, x0, 0x00);
        x1 = _mm512_clmulepi64_epi128(x1, x0, 0x11);
        x1 = _mm512_ternarylogic_epi64(x1, x2, x5, 0x96);

        input += 64;
        length -= 64;
    }

    /*
     * Fold 512-bits to 384-bits.
     */
    a0 = _mm_load_si128((__m128i *)k5k6);

    a1 = _mm512_extracti32x4_epi32(x1, 0);
    a2 = _mm512_extracti32x4_epi32(x1, 1);

    a3 = _mm_clmulepi64_si128(a1, a0, 0x00);
    a1 = _mm_clmulepi64_si128(a1, a0, 0x11);

    a1 = _mm_ternarylogic_epi64(a1, a3, a2, 0x96);

    /*
     * Fold 384-bits to 256-bits.
     */
    a2 = _mm512_extracti32x4_epi32(x1, 2);
    a3 = _mm_clmulepi64_si128(a1, a0, 0x00);
    a1 = _mm_clmulepi64_si128(a1, a0, 0x11);
    a1 = _mm_ternarylogic_epi64(a1, a3, a2, 0x96);

    /*
     * Fold 256-bits to 128-bits.
     */
    a2 = _mm512_extracti32x4_epi32(x1, 3);
    a3 = _mm_clmulepi64_si128(a1, a0, 0x00);
    a1 = _mm_clmulepi64_si128(a1, a0, 0x11);
    a1 = _mm_ternarylogic_epi64(a1, a3, a2, 0x96);

    /*
     * Fold 128-bits to 64-bits.
     */
    a2 = _mm_clmulepi64_si128(a1, a0, 0x10);
    a3 = _mm_setr_epi32(~0, 0, ~0, 0);
    a1 = _mm_srli_si128(a1, 8);
    a1 = _mm_xor_si128(a1, a2);

    a0 = _mm_loadl_epi64((__m128i*)k7k8);
    a2 = _mm_srli_si128(a1, 4);
    a1 = _mm_and_si128(a1, a3);
    a1 = _mm_clmulepi64_si128(a1, a0, 0x00);
    a1 = _mm_xor_si128(a1, a2);

    /*
     * Barret reduce to 32-bits.
     */
    a0 = _mm_load_si128((__m128i*)poly);

    a2 = _mm_and_si128(a1, a3);
    a2 = _mm_clmulepi64_si128(a2, a0, 0x10);
    a2 = _mm_and_si128(a2, a3);
    a2 = _mm_clmulepi64_si128(a2, a0, 0x00);
    a1 = _mm_xor_si128(a1, a2);

    /*
     * Return the crc32.
     */
    return _mm_extract_epi32(a1, 1);
}

static bool detection_performed = false;
static bool detected_clmul = false;
static bool detected_sse42 = false;
static bool detected_avx512 = false;

/*
 * Computes the Castagnoli CRC32c (iSCSI) of the specified data buffer using the Intel CRC32Q (64-bit quad word) and
 * PCLMULQDQ machine instructions (if present).
 * Handles data that isn't 8-byte aligned as well as any trailing data with the CRC32B (byte) instruction.
 * Pass 0 in the previousCrc32 parameter as an initial value unless continuing to update a running CRC in a subsequent
 * call.
 */
uint32_t aws_checksums_crc32c_hw(const uint8_t *input, int length, uint32_t previousCrc32) {

    if (AWS_UNLIKELY(!detection_performed)) {
        detected_clmul = aws_cpu_has_feature(AWS_CPU_FEATURE_CLMUL);
        detected_sse42 = aws_cpu_has_feature(AWS_CPU_FEATURE_SSE_4_2);
        detected_avx512 = aws_cpu_has_feature(AWS_CPU_FEATURE_AVX512);
        /* Simply setting the flag true to skip HW detection next time
           Not using memory barriers since the worst that can
           happen is a fallback to the non HW accelerated code. */
        detection_performed = true;
    }

    uint32_t crc = ~previousCrc32;

    /* For small input, forget about alignment checks - simply compute the CRC32c one byte at a time */
    if (AWS_UNLIKELY(length < 8)) {
        while (length-- > 0) {
            __asm__("CRC32B (%[in]), %[crc]" : [ crc ] "+c"(crc) : [ in ] "r"(input));
            input++;
        }
        return ~crc;
    }

    /* Get the 8-byte memory alignment of our input buffer by looking at the least significant 3 bits */
    int input_alignment = (unsigned long int)input & 0x7;

    /* Compute the number of unaligned bytes before the first aligned 8-byte chunk (will be in the range 0-7) */
    int leading = (8 - input_alignment) & 0x7;

    /* reduce the length by the leading unaligned bytes we are about to process */
    length -= leading;

    /* spin through the leading unaligned input bytes (if any) one-by-one */
    while (leading-- > 0) {
        __asm__("CRC32B (%[in]), %[crc]" : [ crc ] "+c"(crc) : [ in ] "r"(input));
        input++;
    }

    /* Using likely to keep this code inlined */
    if (AWS_LIKELY(detected_clmul)) {
        if (AWS_LIKELY(detected_avx512)) {
            if (AWS_LIKELY(length >= 256)) {
                ssize_t chunk_size = length & ~63;
                crc = ~crc32c_avx512(input, length, crc);
                /* check remaining data */
                length -= chunk_size;
                if (!length)
                    return crc;
                /* Fall into the default crc32 for the remaining data. */
                input += chunk_size;
            }
        }
        else if (AWS_LIKELY(detected_sse42)) {
            while (AWS_LIKELY(length >= 3072)) {
                /* Compute crc32c on each block, chaining each crc result */
                crc = s_crc32c_sse42_clmul_3072(input, crc);
                input += 3072;
                length -= 3072;
            }
            while (AWS_LIKELY(length >= 1024)) {
                /* Compute crc32c on each block, chaining each crc result */
                crc = s_crc32c_sse42_clmul_1024(input, crc);
                input += 1024;
                length -= 1024;
            }
            while (AWS_LIKELY(length >= 256)) {
                /* Compute crc32c on each block, chaining each crc result */
                crc = s_crc32c_sse42_clmul_256(input, crc);
                input += 256;
                length -= 256;
            }
        }
    }

    /* Spin through remaining (aligned) 8-byte chunks using the CRC32Q quad word instruction */
    while (AWS_LIKELY(length >= 8)) {
        /* Hardcoding %rcx register (i.e. "+c") to allow use of qword instruction */
        __asm__ __volatile__("CRC32Q (%[in]), %%rcx" : [ crc ] "+c"(crc) : [ in ] "r"(input));
        input += 8;
        length -= 8;
    }

    /* Finish up with any trailing bytes using the CRC32B single byte instruction one-by-one */
    while (length-- > 0) {
        __asm__ __volatile__("CRC32B (%[in]), %[crc]" : [ crc ] "+c"(crc) : [ in ] "r"(input));
        input++;
    }

    return ~crc;
}
uint32_t aws_checksums_crc32_hw(const uint8_t *input, int length, uint32_t previousCrc32) {
    return aws_checksums_crc32_sw(input, length, previousCrc32);
}

#    if defined(__clang__)
#        pragma clang diagnostic pop
#    endif

#else
uint32_t aws_checksums_crc32_hw(const uint8_t *input, int length, uint32_t previousCrc32) {
    return aws_checksums_crc32_sw(input, length, previousCrc32);
}

uint32_t aws_checksums_crc32c_hw(const uint8_t *input, int length, uint32_t previousCrc32) {
    return aws_checksums_crc32c_sw(input, length, previousCrc32);
}

#endif
/* clang-format on */