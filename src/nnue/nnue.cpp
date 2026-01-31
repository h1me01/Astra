#include <algorithm>
#include <cmath>
#include <cstring> // std::memcpy
#include <fstream>
#include <iostream>
#include <random>

#include "../chess/board.h"
#include "accum.h"
#include "nnue.h"
#include "utils.h"

#include "../third_party/incbin/incbin.h"

#define INCBIN_STYLE INCBIN_STYLE_CAMEL

INCBIN(Weights, NNUE_PATH);

namespace nnue {

// helper for random weights

template <typename T>
T random_weight(std::mt19937& rng, T range) {
    if constexpr (std::is_integral_v<T>) {
        std::uniform_int_distribution<T> dist(-range, range);
        return dist(rng);
    } else {
        std::uniform_real_distribution<T> dist(-range, range);
        return dist(rng);
    }
}

// activation functions

ivec_t crelu(ivec_t val) {
    return simd::clamp_epi16(val, simd::ZERO_IVEC, simd::FT_QUANT_IVEC);
}

fvec_t crelu(fvec_t val) {
    return simd::clamp_ps(val, simd::ZERO_FVEC, simd::ONE_FVEC);
}

// NNUE

void NNUE::init() {
    /*
    std::size_t offset = 0;

    auto load = [&](auto* dest, std::size_t count) {
        const std::size_t bytes = count * sizeof(*dest);
        std::memcpy(dest, &gWeightsData[offset], bytes);
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
    */

    // randomly initialize the weights

    std::mt19937 rng(123456);
    for (auto& w : ft_weights)
        w = random_weight<int16_t>(rng, FT_QUANT * 0.99 - 1);
    for (auto& b : ft_biases)
        b = random_weight<int16_t>(rng, FT_QUANT * 0.99 - 1);
    for (auto& bucket : l1_weights)
        for (auto& w : bucket)
            w = random_weight<int8_t>(rng, L1_QUANT * 0.99 - 1);
    for (auto& bucket : l1_biases)
        for (auto& b : bucket)
            b = random_weight<float>(rng, 0.99f);
    for (auto& bucket : l2_weights)
        for (auto& w : bucket)
            w = random_weight<float>(rng, 0.99f);
    for (auto& bucket : l2_biases)
        for (auto& b : bucket)
            b = random_weight<float>(rng, 0.99f);
    for (auto& bucket : l3_weights)
        for (auto& w : bucket)
            w = random_weight<float>(rng, 0.99f);
    for (auto& b : l3_biases)
        b = random_weight<float>(rng, 0.99f);

    for (size_t i = 0; i < 256; i++) {
        U64 j = i;
        U64 k = 0;
        while (j)
            nnz_lookup[i][k++] = pop_lsb(j);
    }

    simd::permute_simd_data(ptr_cast<__m128i>(ft_biases), FT_SIZE);
    simd::permute_simd_data(ptr_cast<__m128i>(ft_weights), INPUT_SIZE * FT_SIZE);

    for (int b = 0; b < OUTPUT_BUCKETS; b++) {
        int8_t temp_l1_weights[FT_SIZE * L1_SIZE];
        for (int i = 0; i < FT_SIZE / INT8_PER_INT32; i++) {
            for (int j = 0; j < L1_SIZE; j++) {
                for (int k = 0; k < INT8_PER_INT32; k++) {
                    int src_idx = j * FT_SIZE + i * INT8_PER_INT32 + k;
                    int dst_idx = (i * L1_SIZE + j) * INT8_PER_INT32 + k;
                    temp_l1_weights[dst_idx] = l1_weights[b][src_idx];
                }
            }
        }
        std::memcpy(l1_weights[b], temp_l1_weights, sizeof(temp_l1_weights));

        transpose<float>(l2_weights[b], L2_SIZE, L1_SIZE);
    }
}

void NNUE::init_accum(Accum& acc) const {
    std::memcpy(acc.data[WHITE], nnue.ft_biases, sizeof(int16_t) * FT_SIZE);
    std::memcpy(acc.data[BLACK], nnue.ft_biases, sizeof(int16_t) * FT_SIZE);
}

Score NNUE::forward(Board& board, const Accum& acc) {
    assert(acc.initialized[WHITE] && acc.initialized[BLACK]);

    const int bucket = (pop_count(board.occupancy()) - 2) / 4;
    assert(0 <= bucket && bucket < OUTPUT_BUCKETS);

    auto l1_input = prep_l1_input(board.side_to_move(), acc);
    auto l1_output = forward_l1(bucket, l1_input);
    auto l2_output = forward_l2(bucket, l1_output);
    return forward_l3(bucket, l2_output);
}

LayerOutput<uint8_t, FT_SIZE> NNUE::prep_l1_input(const Color stm, const Accum& acc) {
    LayerOutput<uint8_t, FT_SIZE> output;

    for (Color c : {stm, ~stm}) {
        const auto acc_data = acc.data[c];
        const int out_offset = (c == stm) ? 0 : FT_SIZE / 2;

        for (int i = 0; i < FT_SIZE / 2; i += 2 * simd::INT16_VEC_SIZE) {
            ivec_t r_clipped1 = crelu(ivec_at(acc_data, i));
            ivec_t r_clipped2 = crelu(ivec_at(acc_data, i + simd::INT16_VEC_SIZE));

            ivec_t l_clipped1 = min_epi16(ivec_at(acc_data, i + FT_SIZE / 2), simd::FT_QUANT_IVEC);
            ivec_t l_clipped2 =
                min_epi16(ivec_at(acc_data, i + FT_SIZE / 2 + simd::INT16_VEC_SIZE), simd::FT_QUANT_IVEC);
            ivec_t shifted1 = slli_epi16(r_clipped1, 16 - FT_SHIFT);
            ivec_t shifted2 = slli_epi16(r_clipped2, 16 - FT_SHIFT);

            ivec_t product1 = mulhi_epi16(shifted1, l_clipped1);
            ivec_t product2 = mulhi_epi16(shifted2, l_clipped2);

            ivec_at(output, i + out_offset) = packus_epi16(product1, product2);
        }
    }

    return output;
}

NNUE::NNZOutput NNUE::find_nnz(const LayerOutput<uint8_t, FT_SIZE>& input) {
    int count = 0;
    alignas(ALIGNMENT) LayerOutput<uint16_t, FT_SIZE / 4> indices;

    const __m128i inc = _mm_set1_epi16(8);
    __m128i base = _mm_setzero_si128();

    for (int i = 0; i < FT_SIZE; i += 2 * simd::INT16_VEC_SIZE) {
        uint32_t nnz = simd::nnz_non_zero_mask(ivec_at(input, i));

        for (int j = 0; j < simd::INT32_VEC_SIZE; j += 8) {
            uint16_t lookup = (nnz >> j) & 0xFF;
            __m128i offsets = _mm_loadu_si128(ptr_cast<__m128i>(&nnz_lookup[lookup]));
            _mm_storeu_si128(ptr_cast<__m128i>(indices + count), _mm_add_epi16(base, offsets));

            count += __builtin_popcount(lookup);
            base = _mm_add_epi16(base, inc);
        }
    }

    return {count, indices};
}

LayerOutput<float, L1_SIZE> NNUE::forward_l1(int bucket, const LayerOutput<uint8_t, FT_SIZE>& input) {
    LayerOutput<float, L1_SIZE> output;

    const auto input_packs = ptr_cast<int32_t>(input);
    auto [nnz_count, nnz_indices] = find_nnz(input);

    alignas(ALIGNMENT) ivec_t linear[L1_SIZE / simd::INT32_VEC_SIZE];
    std::fill_n(linear, L1_SIZE / simd::INT32_VEC_SIZE, simd::ZERO_IVEC);

    int i = 0;
    for (; i < nnz_count - 1; i += 2) {
        const int idx1 = nnz_indices[i];
        const int idx2 = nnz_indices[i + 1];

        const auto input1 = set1_epi32(input_packs[idx1]);
        const auto input2 = set1_epi32(input_packs[idx2]);

        const auto weights1 = &l1_weights[bucket][idx1 * L1_SIZE * INT8_PER_INT32];
        const auto weights2 = &l1_weights[bucket][idx2 * L1_SIZE * INT8_PER_INT32];

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

    for (; i < nnz_count; i++) {
        const int idx = nnz_indices[i];
        const auto input = set1_epi32(input_packs[idx]);
        const auto weights = &l1_weights[bucket][idx * L1_SIZE * INT8_PER_INT32];

        for (int j = 0; j < L1_SIZE; j += simd::INT32_VEC_SIZE) {
            int o_offset = j / simd::INT32_VEC_SIZE;
            linear[o_offset] = simd::dpbusd_epi32(linear[o_offset], input, ivec_at(weights, j * INT8_PER_INT32));
        }
    }

    // activate and add bias to value
    for (int i = 0; i < L1_SIZE; i += simd::FLOAT_VEC_SIZE) {
        auto converted_linear = cvtepi32_ps(ivec_at(linear, i / simd::INT32_VEC_SIZE));
        auto l1_out = fmadd_ps(converted_linear, simd::DEQUANT_MULT_PS, fvec_at(l1_biases[bucket], i));
        fvec_at(output, i) = crelu(l1_out);
    }

    return output;
}

LayerOutput<float, L2_SIZE> NNUE::forward_l2(int bucket, const LayerOutput<float, L1_SIZE>& input) {
    LayerOutput<float, L2_SIZE> output(l2_biases[bucket]);

    for (int i = 0; i < L1_SIZE; i++) {
        const auto input_val = set1_ps(input[i]);
        const auto weights = ptr_cast<const float>(&l2_weights[bucket][i * L2_SIZE]);

        for (int j = 0; j < L2_SIZE; j += simd::FLOAT_VEC_SIZE)
            fvec_at(output, j) = fmadd_ps(fvec_at(weights, j), input_val, fvec_at(output, j));
    }

    for (int i = 0; i < L2_SIZE; i += simd::FLOAT_VEC_SIZE)
        fvec_at(output, i) = crelu(fvec_at(output, i));

    return output;
}

float NNUE::forward_l3(int bucket, const LayerOutput<float, L2_SIZE>& input) {
    constexpr int VEC_SIZE = 16 / simd::FLOAT_VEC_SIZE;

    fvec_t output[VEC_SIZE];
    std::fill_n(output, VEC_SIZE, simd::ZERO_FVEC);

    for (int i = 0; i < VEC_SIZE; i++) {
        for (int j = 0; j < L2_SIZE; j += VEC_SIZE * simd::FLOAT_VEC_SIZE) {
            int idx = j + i * simd::FLOAT_VEC_SIZE;
            output[i] = fmadd_ps(fvec_at(l3_weights[bucket], idx), fvec_at(input, idx), output[i]);
        }
    }

    return (simd::hor_sum_ps(output) + l3_biases[bucket]) * EVAL_SCALE;
}

void NNUE::put(Accum& acc, const Accum& prev, Piece pc, Square psq, Square ksq, Color view) const {
    const int idx = feature_idx(pc, psq, ksq, view);
    const auto weights = ptr_cast<const ivec_t>(&ft_weights[idx * FT_SIZE]);

    auto acc_data = ptr_cast<ivec_t>(acc.data[view]);
    const auto prev_data = acc.initialized[view] ? acc_data : ptr_cast<const ivec_t>(prev.data[view]);

    for (int i = 0; i < FT_SIZE / simd::INT16_VEC_SIZE; i++)
        acc_data[i] = add_epi16(prev_data[i], weights[i]);

    acc.initialized[view] = true;
}

void NNUE::remove(Accum& acc, const Accum& prev, Piece pc, Square psq, Square ksq, Color view) const {
    const int idx = feature_idx(pc, psq, ksq, view);
    const auto weights = ptr_cast<const ivec_t>(&ft_weights[idx * FT_SIZE]);

    auto acc_data = ptr_cast<ivec_t>(acc.data[view]);
    const auto prev_data = acc.initialized[view] ? acc_data : ptr_cast<const ivec_t>(prev.data[view]);

    for (int i = 0; i < FT_SIZE / simd::INT16_VEC_SIZE; i++)
        acc_data[i] = sub_epi16(prev_data[i], weights[i]);

    acc.initialized[view] = true;
}

void NNUE::move(Accum& acc, const Accum& prev, Piece pc, Square from, Square to, Square ksq, Color view) const {
    const int from_idx = feature_idx(pc, from, ksq, view);
    const int to_idx = feature_idx(pc, to, ksq, view);
    const auto weights_from = ptr_cast<const ivec_t>(&ft_weights[from_idx * FT_SIZE]);
    const auto weights_to = ptr_cast<const ivec_t>(&ft_weights[to_idx * FT_SIZE]);

    auto acc_data = ptr_cast<ivec_t>(acc.data[view]);
    const auto prev_data = acc.initialized[view] ? acc_data : ptr_cast<const ivec_t>(prev.data[view]);

    for (int i = 0; i < FT_SIZE / simd::INT16_VEC_SIZE; i++)
        acc_data[i] = add_epi16(prev_data[i], sub_epi16(weights_to[i], weights_from[i]));

    acc.initialized[view] = true;
}

// Global Variable

NNUE nnue;

} // namespace nnue
