#include <cstring>

#include "../../third_party/incbin/incbin.h"

#include "../chess/board.h"
#include "accumulator.h"
#include "nnue.h"

#define INCBIN_STYLE INCBIN_STYLE_CAMEL

INCBIN(Weights, NNUE_PATH);

namespace astra::nnue {

constexpr int FT_SHIFT = 9;
constexpr int INT8_PER_INT32 = sizeof(int32_t) / sizeof(int8_t);

void NNUE::init() {
    size_t offset = 0;

    auto load = [&](auto* dest, size_t count) {
        size_t bytes = count * sizeof(*dest);
        std::memcpy(dest, &gWeightsData[offset], bytes);
        offset += bytes;
    };

    load(ft_weight_.data(), INPUT_SIZE * FT_SIZE);
    load(ft_bias_.data(), FT_SIZE);
    load(l1_weight_.data(), OUTPUT_BUCKETS * FT_SIZE * L1_SIZE);
    load(l1_bias_.data(), OUTPUT_BUCKETS * L1_SIZE);
    load(l2_weight_.data(), OUTPUT_BUCKETS * 2 * L1_SIZE * L2_SIZE);
    load(l2_bias_.data(), OUTPUT_BUCKETS * L2_SIZE);
    load(l3_weight_.data(), OUTPUT_BUCKETS * L2_SIZE);
    load(l3_bias_.data(), OUTPUT_BUCKETS);

    for (size_t i = 0; i < 256; ++i) {
        uint64_t j = i;
        uint64_t k = 0;
        while (j)
            nnz_lookup_(i, k++) = pop_lsb(j);
    }

    simd::permute_simd_data(ptr_cast<__m128i>(ft_bias_.data()), FT_SIZE);
    simd::permute_simd_data(ptr_cast<__m128i>(ft_weight_.data()), INPUT_SIZE * FT_SIZE);

    for (int b = 0; b < OUTPUT_BUCKETS; ++b) {
        int8_t temp_l1_weights[FT_SIZE * L1_SIZE];
        for (int i = 0; i < FT_SIZE / INT8_PER_INT32; ++i) {
            for (int j = 0; j < L1_SIZE; ++j) {
                for (int k = 0; k < INT8_PER_INT32; ++k) {
                    int src_idx = j * FT_SIZE + i * INT8_PER_INT32 + k;
                    int dst_idx = (i * L1_SIZE + j) * INT8_PER_INT32 + k;
                    temp_l1_weights[dst_idx] = l1_weight_(b, src_idx);
                }
            }
        }
        std::memcpy(&l1_weight_(b, 0), temp_l1_weights, sizeof(temp_l1_weights));

        transpose<float>(&l2_weight_(b, 0), L2_SIZE, 2 * L1_SIZE);
    }
}

int32_t NNUE::forward(Board& board, const Accumulator& acc) {
    assert(acc.initialized(WHITE));
    assert(acc.initialized(BLACK));

    const int bucket = (pop_count(board.occupancy()) - 2) / 4;
    assert(0 <= bucket && bucket < OUTPUT_BUCKETS);

    alignas(64) auto l1_in = prep_l1_input(board.side_to_move(), acc);
    alignas(64) auto l1_out = forward_l1(bucket, l1_in);
    alignas(64) auto l2_out = forward_l2(bucket, l1_out);
    return forward_l3(bucket, l2_out) * EVAL_SCALE;
}

NDArray<uint8_t, FT_SIZE> NNUE::prep_l1_input(const Color stm, const Accumulator& acc) {
    alignas(64) NDArray<uint8_t, FT_SIZE> output;

    const simd::ivec_t FT_QUANT_IVEC = simd::set1_epi16(FT_QUANT);

    auto crelu = [&](simd::ivec_t val) { return simd::clamp_epi16(val, simd::zero_ivec(), FT_QUANT_IVEC); };

    for (Color c : {stm, ~stm}) {
        const auto acc_data = &acc.data(c, 0);
        const int out_offset = (c == stm) ? 0 : FT_SIZE / 2;

        for (int i = 0; i < FT_SIZE / 2; i += 2 * simd::INT16_VEC_SIZE) {
            simd::ivec_t r_clipped1 = crelu(ivec_at(acc_data, i));
            simd::ivec_t r_clipped2 = crelu(ivec_at(acc_data, i + simd::INT16_VEC_SIZE));

            simd::ivec_t l_clipped1 = simd::min_epi16(ivec_at(acc_data, i + FT_SIZE / 2), FT_QUANT_IVEC);
            simd::ivec_t l_clipped2 =
                simd::min_epi16(ivec_at(acc_data, i + FT_SIZE / 2 + simd::INT16_VEC_SIZE), FT_QUANT_IVEC);
            simd::ivec_t shifted1 = simd::slli_epi16(r_clipped1, 16 - FT_SHIFT);
            simd::ivec_t shifted2 = simd::slli_epi16(r_clipped2, 16 - FT_SHIFT);

            simd::ivec_t product1 = simd::mulhi_epi16(shifted1, l_clipped1);
            simd::ivec_t product2 = simd::mulhi_epi16(shifted2, l_clipped2);

            ivec_at(&output(i + out_offset)) = simd::packus_epi16(product1, product2);
        }
    }

    return output;
}

NNUE::NNZOutput NNUE::find_nnz(const NDArray<uint8_t, FT_SIZE>& input) {
    int count = 0;
    alignas(64) NDArray<uint16_t, FT_SIZE / 4> indices;

    const __m128i inc = _mm_set1_epi16(8);
    __m128i base = _mm_setzero_si128();

    for (int i = 0; i < FT_SIZE; i += 2 * simd::INT16_VEC_SIZE) {
        uint32_t nnz = simd::nnz_non_zero_mask(ivec_at(&input(i)));

        for (int j = 0; j < simd::INT32_VEC_SIZE; j += 8) {
            uint16_t lookup = (nnz >> j) & 0xFF;
            __m128i offsets = _mm_loadu_si128(ptr_cast<__m128i>(&nnz_lookup_(lookup, 0)));
            _mm_storeu_si128(ptr_cast<__m128i>(&indices(count)), _mm_add_epi16(base, offsets));

            count += __builtin_popcount(lookup);
            base = _mm_add_epi16(base, inc);
        }
    }

    return {count, indices};
}

NDArray<float, 2 * L1_SIZE> NNUE::forward_l1(int bucket, const NDArray<uint8_t, FT_SIZE>& input) {
    alignas(64) NDArray<float, 2 * L1_SIZE> output;

    const auto input_packs = ptr_cast<int32_t>(input.data());
    auto [nnz_count, nnz_indices] = find_nnz(input);

    alignas(64) simd::ivec_t linear[L1_SIZE / simd::INT32_VEC_SIZE];
    std::memset(linear, 0, sizeof(linear));

    int i = 0;
    for (; i < nnz_count - 1; i += 2) {
        const int idx1 = nnz_indices(i);
        const int idx2 = nnz_indices(i + 1);

        const auto input1 = simd::set1_epi32(input_packs[idx1]);
        const auto input2 = simd::set1_epi32(input_packs[idx2]);

        const auto weights1 = &l1_weight_(bucket, idx1 * L1_SIZE * INT8_PER_INT32);
        const auto weights2 = &l1_weight_(bucket, idx2 * L1_SIZE * INT8_PER_INT32);

        for (int j = 0; j < L1_SIZE; j += simd::INT32_VEC_SIZE) {
            int o_offset = j / simd::INT32_VEC_SIZE;
            linear[o_offset] = simd::double_dpbusd_epi32(
                linear[o_offset],
                input1,
                ivec_at(weights1, j * INT8_PER_INT32),
                input2,
                ivec_at(weights2, j * INT8_PER_INT32)
            );
        }
    }

    for (; i < nnz_count; ++i) {
        const int idx = nnz_indices(i);
        const auto input = simd::set1_epi32(input_packs[idx]);
        const auto weights = &l1_weight_(bucket, idx * L1_SIZE * INT8_PER_INT32);

        for (int j = 0; j < L1_SIZE; j += simd::INT32_VEC_SIZE) {
            int o_offset = j / simd::INT32_VEC_SIZE;
            linear[o_offset] = simd::dpbusd_epi32(linear[o_offset], input, ivec_at(weights, j * INT8_PER_INT32));
        }
    }

    const simd::fvec_t DEQUANT_MULT_PS =
        simd::set1_ps((1 << FT_SHIFT) / static_cast<float>(FT_QUANT * FT_QUANT * L1_QUANT));

    // activate and add bias to value
    for (int i = 0; i < L1_SIZE; i += simd::FLOAT_VEC_SIZE) {
        auto converted_linear = simd::cvtepi32_ps(ivec_at(linear, i / simd::INT32_VEC_SIZE));
        auto l1_out = simd::fmadd_ps(converted_linear, DEQUANT_MULT_PS, fvec_at(&l1_bias_(bucket, i)));
        fvec_at(&output(i)) = simd::clamp_ps(l1_out, simd::zero_fvec(), simd::set1_ps(1.0f));
        fvec_at(&output(i + L1_SIZE)) = simd::min_ps(simd::mul_ps(l1_out, l1_out), simd::set1_ps(1.0f));
    }

    return output;
}

NDArray<float, L2_SIZE> NNUE::forward_l2(int bucket, const NDArray<float, 2 * L1_SIZE>& input) {
    alignas(64) NDArray<float, L2_SIZE> output;
    std::memcpy(output.data(), &l2_bias_(bucket, 0), sizeof(float) * L2_SIZE);

    for (int i = 0; i < 2 * L1_SIZE; ++i) {
        const auto input_val = simd::set1_ps(input(i));
        const auto weights = ptr_cast<const float>(&l2_weight_(bucket, i * L2_SIZE));

        for (int j = 0; j < L2_SIZE; j += simd::FLOAT_VEC_SIZE)
            fvec_at(&output(j)) = simd::fmadd_ps(fvec_at(weights, j), input_val, fvec_at(&output(j)));
    }

    for (int i = 0; i < L2_SIZE; i += simd::FLOAT_VEC_SIZE)
        fvec_at(&output(i)) = simd::clamp_ps(fvec_at(&output(i)), simd::zero_fvec(), simd::set1_ps(1.0f));

    return output;
}

float NNUE::forward_l3(int bucket, const NDArray<float, L2_SIZE>& input) {
    const int vec_size = 16 / simd::FLOAT_VEC_SIZE;

    simd::fvec_t output[vec_size];
    std::memset(output, 0, sizeof(output));

    for (int i = 0; i < vec_size; ++i) {
        for (int j = 0; j < L2_SIZE; j += vec_size * simd::FLOAT_VEC_SIZE) {
            int idx = j + i * simd::FLOAT_VEC_SIZE;
            output[i] = simd::fmadd_ps(fvec_at(&l3_weight_(bucket, idx)), fvec_at(&input(idx)), output[i]);
        }
    }

    return simd::hor_sum_ps(output) + l3_bias_(bucket);
}

NNUE nnue;

} // namespace astra::nnue
