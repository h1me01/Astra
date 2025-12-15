#include <algorithm>
#include <cstring> // std::memcpy
#include <fstream>
#include <iostream>

#include "../chess/board.h"
#include "accum.h"
#include "nnue.h"
#include "utils.h"

#include "../third_party/incbin/incbin.h"

#define INCBIN_STYLE INCBIN_STYLE_CAMEL

INCBIN(Weights, NNUE_PATH);

namespace nnue {

// activation functions

#ifdef USE_SIMD

ivec_t crelu(ivec_t val) {
    return simd::clamp_epi16(val, simd::ZERO_IVEC, simd::FT_QUANT_IVEC);
}

fvec_t crelu(fvec_t val) {
    return simd::clamp_ps(val, simd::ZERO_FVEC, simd::ONE_FVEC);
}

#else

int32_t crelu(int16_t x) {
    return std::clamp(int32_t(x), 0, FT_QUANT);
}

float crelu(float x) {
    return std::clamp(x, 0.0f, 1.0f);
}

#endif

// NNUE

void NNUE::init() {
    std::size_t offset = 0;

    auto load = [&](auto *dest, std::size_t count) {
        const std::size_t bytes = count * sizeof(*dest);
        memcpy(dest, &gWeightsData[offset], bytes);
        offset += bytes;
    };

    load(&ft_weights[0], INPUT_SIZE * FT_SIZE);
    load(&ft_biases[0], FT_SIZE);
    load(&l1_weights[0][0], OUTPUT_BUCKETS * FT_SIZE * L1_SIZE);
    load(&l1_biases[0][0], OUTPUT_BUCKETS * L1_SIZE);
    load(&l2_weights[0][0], OUTPUT_BUCKETS * L1_SIZE * L2_SIZE);
    load(&l2_biases[0][0], OUTPUT_BUCKETS * L2_SIZE);
    load(&l3_weights[0][0], OUTPUT_BUCKETS * L2_SIZE);
    load(&l3_biases[0], OUTPUT_BUCKETS);

    for(size_t i = 0; i < 256; i++) {
        U64 j = i;
        U64 k = 0;
        while(j)
            nnz_lookup[i][k++] = pop_lsb(j);
    }

#ifdef USE_SIMD
    permute_simd_data(ptr_cast<__m128i>(ft_biases), FT_SIZE);
    permute_simd_data(ptr_cast<__m128i>(ft_weights), INPUT_SIZE * FT_SIZE);
#endif

    for(int b = 0; b < OUTPUT_BUCKETS; b++) {
#ifdef USE_SIMD
        int8_t temp_l1_weights[FT_SIZE * L1_SIZE];

        for(int i = 0; i < FT_SIZE / INT8_PER_INT32; i++) {
            for(int j = 0; j < L1_SIZE; j++) {
                for(int k = 0; k < INT8_PER_INT32; k++) {
                    int src_idx = j * FT_SIZE + i * INT8_PER_INT32 + k;
                    int dst_idx = (i * L1_SIZE + j) * INT8_PER_INT32 + k;
                    temp_l1_weights[dst_idx] = l1_weights[b][src_idx];
                }
            }
        }

        memcpy(l1_weights[b], temp_l1_weights, sizeof(temp_l1_weights));
#else
        transpose<int8_t>(l1_weights[b], L1_SIZE, FT_SIZE);
#endif

        transpose<float>(l2_weights[b], L2_SIZE, L1_SIZE);
    }
}

void NNUE::init_accum(Accum &acc) const {
    memcpy(acc.get_data(WHITE), nnue.ft_biases, sizeof(int16_t) * FT_SIZE);
    memcpy(acc.get_data(BLACK), nnue.ft_biases, sizeof(int16_t) * FT_SIZE);
}

int32_t NNUE::forward(Board &board, const Accum &acc) {
    assert(acc.is_initialized(WHITE) && acc.is_initialized(BLACK));

    const int bucket = (pop_count(board.get_occupancy()) - 2) / 4;
    assert(0 <= bucket && bucket < OUTPUT_BUCKETS);

    auto l1_input = prep_l1_input(board.get_stm(), acc);
    auto l1_output = forward_l1(bucket, l1_input);
    auto l2_output = forward_l2(bucket, l1_output);
    return static_cast<int32_t>(forward_l3(bucket, l2_output));
}

LayerOutput<uint8_t, FT_SIZE> NNUE::prep_l1_input(const Color stm, const Accum &acc) {
    LayerOutput<uint8_t, FT_SIZE> output;

    for(Color c : {stm, ~stm}) {
        const auto acc_data = acc.get_data(c);
        const int out_offset = (c == stm) ? 0 : FT_SIZE / 2;

#ifdef USE_SIMD
        for(int i = 0; i < FT_SIZE / 2; i += 2 * simd::INT16_VEC_SIZE) {
            ivec_t r_clipped1 = crelu(ivec_at(acc_data, i));
            ivec_t r_clipped2 = crelu(ivec_at(acc_data, i + simd::INT16_VEC_SIZE));

            ivec_t l_clipped1 = min_epi16(ivec_at(acc_data, i + FT_SIZE / 2), simd::FT_QUANT_IVEC);
            ivec_t l_clipped2 = min_epi16(ivec_at(acc_data, i + FT_SIZE / 2 + simd::INT16_VEC_SIZE), simd::FT_QUANT_IVEC);
            ivec_t shifted1 = slli_epi16(r_clipped1, 16 - FT_SHIFT);
            ivec_t shifted2 = slli_epi16(r_clipped2, 16 - FT_SHIFT);

            ivec_t product1 = mulhi_epi16(shifted1, l_clipped1);
            ivec_t product2 = mulhi_epi16(shifted2, l_clipped2);

            store_si(ptr_cast<ivec_t>(&output[i + out_offset]), packus_epi16(product1, product2));
        }
#else
        for(int i = 0; i < FT_SIZE / 2; i++) {
            int32_t crelu1 = crelu(acc_data[i]);
            int32_t crelu2 = crelu(acc_data[i + FT_SIZE / 2]);
            output[i + out_offset] = (crelu1 * crelu2) >> FT_SHIFT;
        }
#endif
    }

    return output;
}

NNUE::NNZOutput NNUE::find_nnz([[maybe_unused]] const LayerOutput<uint8_t, FT_SIZE> &input) {
    int count = 0;
    alignas(ALIGNMENT) LayerOutput<uint16_t, FT_SIZE / 4> indices;

#ifdef USE_SIMD
    const __m128i inc = _mm_set1_epi16(8);
    __m128i base = _mm_setzero_si128();

    for(int i = 0; i < FT_SIZE; i += 2 * simd::INT16_VEC_SIZE) {
        uint32_t nnz = simd::nnz_nonzero_mask(ivec_at(input, i));

        for(int j = 0; j < simd::INT32_VEC_SIZE; j += 8) {
            uint16_t lookup = (nnz >> j) & 0xFF;
            __m128i offsets = _mm_loadu_si128(ptr_cast<__m128i>(&nnz_lookup[lookup]));
            _mm_storeu_si128(ptr_cast<__m128i>(indices + count), _mm_add_epi16(base, offsets));

            count += __builtin_popcount(lookup);
            base = _mm_add_epi16(base, inc);
        }
    }
#endif

    return {count, indices};
}

LayerOutput<float, L1_SIZE> NNUE::forward_l1(int bucket, const LayerOutput<uint8_t, FT_SIZE> &input) {
    LayerOutput<float, L1_SIZE> output;

#ifdef USE_SIMD
    LayerOutput<int32_t, L1_SIZE> linear;

    auto linear_vec = ptr_cast<ivec_t>(linear);
    auto [nnz_count, nnz_indices] = find_nnz(input);

    const auto input_packs = ptr_cast<int>(input);

    int i = 0;
    for(; i < nnz_count - 1; i += 2) {
        const int idx1 = nnz_indices[i];
        const int idx2 = nnz_indices[i + 1];

        const ivec_t input1 = set1_epi32(input_packs[idx1]);
        const ivec_t input2 = set1_epi32(input_packs[idx2]);

        const auto weights1 = ptr_cast<const int8_t>(&l1_weights[bucket][idx1 * L1_SIZE * INT8_PER_INT32]);
        const auto weights2 = ptr_cast<const int8_t>(&l1_weights[bucket][idx2 * L1_SIZE * INT8_PER_INT32]);

        for(int j = 0; j < L1_SIZE; j += simd::INT32_VEC_SIZE) {
            int o_offset = j / simd::INT32_VEC_SIZE;
            linear_vec[o_offset] = simd::double_dpbusd_epi32(  //
                linear_vec[o_offset], input1,                  //
                ivec_at(weights1, j * INT8_PER_INT32), input2, //
                ivec_at(weights2, j * INT8_PER_INT32)          //
            );
        }
    }

    for(; i < nnz_count; i++) {
        const int idx = nnz_indices[i];
        const ivec_t input = set1_epi32(input_packs[idx]);
        const auto weights = ptr_cast<const int8_t>(&l1_weights[bucket][idx * L1_SIZE * INT8_PER_INT32]);

        for(int j = 0; j < L1_SIZE; j += simd::INT32_VEC_SIZE) {
            int o_offset = j / simd::INT32_VEC_SIZE;
            linear_vec[o_offset] = simd::dpbusd_epi32(      //
                linear_vec[o_offset],                       //
                input, ivec_at(weights, j * INT8_PER_INT32) //
            );
        }
    }

    // activate and add bias to value
    for(int i = 0; i < L1_SIZE; i += simd::FLOAT_VEC_SIZE) {
        fvec_t l1_out = add_ps(                                             //
            mul_ps(cvtepi32_ps(ivec_at(linear, i)), simd::DEQUANT_MULT_PS), //
            fvec_at(&l1_biases[bucket][0], i)                               //
        );

        fvec_at(output, i) = crelu(l1_out);
    }

#else
    for(int i = 0; i < L1_SIZE; i++) {
        for(int j = 0; j < FT_SIZE; j++)
            output[i] += input[j] * l1_weights[bucket][j * L1_SIZE + i];
        output[i] = crelu(output[i] * DEQUANT_MULT + l1_biases[bucket][i]);
    }
#endif

    return output;
}

LayerOutput<float, L2_SIZE> NNUE::forward_l2(int bucket, const LayerOutput<float, L1_SIZE> &input) {
    LayerOutput<float, L2_SIZE> output;

#ifdef USE_SIMD
    output.init(l2_biases[bucket]);

    for(int i = 0; i < L1_SIZE; i++) {
        const fvec_t input_val = set1_ps(input[i]);
        const auto weights = ptr_cast<const float>(&l2_weights[bucket][i * L2_SIZE]);

        for(int j = 0; j < L2_SIZE; j += simd::FLOAT_VEC_SIZE)
            fvec_at(output, j) = fmadd_ps(input_val, fvec_at(weights, j), fvec_at(output, j));
    }

    for(int i = 0; i < L2_SIZE; i += simd::FLOAT_VEC_SIZE)
        fvec_at(output, i) = crelu(fvec_at(output, i));
#else
    for(int i = 0; i < L2_SIZE; i++)
        for(int j = 0; j < L1_SIZE; j++)
            output[i] += input[j] * l2_weights[bucket][j * L2_SIZE + i];

    for(int i = 0; i < L2_SIZE; i++)
        output[i] = crelu(output[i] + l2_biases[bucket][i]);
#endif

    return output;
}

float NNUE::forward_l3(int bucket, const LayerOutput<float, L2_SIZE> &input) {
#ifdef USE_SIMD
    constexpr int VEC_SIZE = 16 / simd::FLOAT_VEC_SIZE;

    fvec_t output[VEC_SIZE];
    for(int i = 0; i < VEC_SIZE; i++)
        output[i] = simd::ZERO_FVEC;

    for(int i = 0; i < VEC_SIZE; i++) {
        for(int j = 0; j < L2_SIZE; j += VEC_SIZE * simd::FLOAT_VEC_SIZE) {
            int offset = j + i * simd::FLOAT_VEC_SIZE;
            output[i] = fmadd_ps(fvec_at(input, offset), fvec_at(&l3_weights[bucket][0], offset), output[i]);
        }
    }

    return (l3_biases[bucket] + simd::hor_sum_ps(output)) * EVAL_SCALE;
#else
    float result = 0;
    for(int i = 0; i < L2_SIZE; i++)
        result += input[i] * l3_weights[bucket][i];

    return (result + l3_biases[bucket]) * static_cast<float>(EVAL_SCALE);
#endif
}

void NNUE::put(Accum &acc, const Accum &prev, Piece pc, Square psq, Square ksq, Color view) const {
    const int idx = feature_idx(psq, ksq, pc, view);
    const auto weights = ptr_cast<const ivec_t>(&ft_weights[idx * FT_SIZE]);

    auto acc_data = ptr_cast<ivec_t>(acc.get_data(view));
    const auto prev_data = acc.is_initialized(view) ? acc_data : ptr_cast<const ivec_t>(prev.get_data(view));

    for(int i = 0; i < FT_SIZE / simd::INT16_VEC_SIZE; i++)
        acc_data[i] = simd::accum_add(prev_data[i], weights[i]);

    acc.set_initialized(view);
}

void NNUE::remove(Accum &acc, const Accum &prev, Piece pc, Square psq, Square ksq, Color view) const {
    const int idx = feature_idx(psq, ksq, pc, view);
    const auto weights = ptr_cast<const ivec_t>(&ft_weights[idx * FT_SIZE]);

    auto acc_data = ptr_cast<ivec_t>(acc.get_data(view));
    const auto prev_data = acc.is_initialized(view) ? acc_data : ptr_cast<const ivec_t>(prev.get_data(view));

    for(int i = 0; i < FT_SIZE / simd::INT16_VEC_SIZE; i++)
        acc_data[i] = simd::accum_sub(prev_data[i], weights[i]);

    acc.set_initialized(view);
}

void NNUE::move(Accum &acc, const Accum &prev, Piece pc, Square from, Square to, Square ksq, Color view) const {
    const int from_idx = feature_idx(from, ksq, pc, view);
    const int to_idx = feature_idx(to, ksq, pc, view);
    const auto weights_from = ptr_cast<const ivec_t>(&ft_weights[from_idx * FT_SIZE]);
    const auto weights_to = ptr_cast<const ivec_t>(&ft_weights[to_idx * FT_SIZE]);

    auto acc_data = ptr_cast<ivec_t>(acc.get_data(view));
    const auto prev_data = acc.is_initialized(view) ? acc_data : ptr_cast<const ivec_t>(prev.get_data(view));

    for(int i = 0; i < FT_SIZE / simd::INT16_VEC_SIZE; i++)
        acc_data[i] = simd::accum_add(prev_data[i], simd::accum_sub(weights_to[i], weights_from[i]));

    acc.set_initialized(view);
}

// global variable

NNUE nnue;

} // namespace nnue
