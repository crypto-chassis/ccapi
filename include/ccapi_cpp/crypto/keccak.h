/* ethash: C/C++ implementation of Ethash, the Ethereum Proof of Work algorithm.
 * Copyright 2018-2019 Pawel Bylica.
 * Licensed under the Apache License, Version 2.0.
 */

#pragma once

#include "attributes.h"
#include "hash_types.h"

#include <stddef.h>

#ifndef __cplusplus
#define noexcept  // Ignore noexcept in C code.
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Provide __has_builtin macro if not defined.
#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

#if !__has_builtin(__builtin_memcpy) && !defined(__GNUC__)
#include <string.h>
#define __builtin_memcpy memcpy
#endif

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
static inline ALWAYS_INLINE uint64_t ethash_to_le64(uint64_t x)
{
    return __builtin_bswap64(x);
}
#else
static inline ALWAYS_INLINE uint64_t ethash_to_le64(uint64_t x)
{
    return x;
}
#endif

// Loads 64-bit integer from given memory location as little-endian number.
static inline ALWAYS_INLINE uint64_t ethash_load_le_u64(const uint8_t* data)
{
    uint64_t word;
    __builtin_memcpy(&word, data, sizeof(word));
    return ethash_to_le64(word);
}

// Rotates the bits of x left by the count value specified by s.
// The s must be in range <0, 64> exclusively, otherwise the result is undefined.
static inline uint64_t ethash_rol_u64(uint64_t x, unsigned s)
{
    return (x << s) | (x >> (64 - s));
}

static const uint64_t ethash_round_constants[24] = {
    0x0000000000000001, 0x0000000000008082, 0x800000000000808a, 0x8000000080008000,
    0x000000000000808b, 0x0000000080000001, 0x8000000080008081, 0x8000000000008009,
    0x000000000000008a, 0x0000000000000088, 0x0000000080008009, 0x000000008000000a,
    0x000000008000808b, 0x800000000000008b, 0x8000000000008089, 0x8000000000008003,
    0x8000000000008002, 0x8000000000000080, 0x000000000000800a, 0x800000008000000a,
    0x8000000080008081, 0x8000000000008080, 0x0000000080000001, 0x8000000080008008};

// The Keccak-f[1600] function (inline header-only implementation).
static inline ALWAYS_INLINE void ethash_keccakf1600_implementation(uint64_t state[25])
{
    uint64_t Aba, Abe, Abi, Abo, Abu;
    uint64_t Aga, Age, Agi, Ago, Agu;
    uint64_t Aka, Ake, Aki, Ako, Aku;
    uint64_t Ama, Ame, Ami, Amo, Amu;
    uint64_t Asa, Ase, Asi, Aso, Asu;

    uint64_t Eba, Ebe, Ebi, Ebo, Ebu;
    uint64_t Ega, Ege, Egi, Ego, Egu;
    uint64_t Eka, Eke, Eki, Eko, Eku;
    uint64_t Ema, Eme, Emi, Emo, Emu;
    uint64_t Esa, Ese, Esi, Eso, Esu;

    uint64_t Ba, Be, Bi, Bo, Bu;

    uint64_t Da, De, Di, Do, Du;

    Aba = state[0];
    Abe = state[1];
    Abi = state[2];
    Abo = state[3];
    Abu = state[4];
    Aga = state[5];
    Age = state[6];
    Agi = state[7];
    Ago = state[8];
    Agu = state[9];
    Aka = state[10];
    Ake = state[11];
    Aki = state[12];
    Ako = state[13];
    Aku = state[14];
    Ama = state[15];
    Ame = state[16];
    Ami = state[17];
    Amo = state[18];
    Amu = state[19];
    Asa = state[20];
    Ase = state[21];
    Asi = state[22];
    Aso = state[23];
    Asu = state[24];

    for (size_t n = 0; n < 24; n += 2)
    {
        // Round (n + 0): Axx -> Exx

        Ba = Aba ^ Aga ^ Aka ^ Ama ^ Asa;
        Be = Abe ^ Age ^ Ake ^ Ame ^ Ase;
        Bi = Abi ^ Agi ^ Aki ^ Ami ^ Asi;
        Bo = Abo ^ Ago ^ Ako ^ Amo ^ Aso;
        Bu = Abu ^ Agu ^ Aku ^ Amu ^ Asu;

        Da = Bu ^ ethash_rol_u64(Be, 1);
        De = Ba ^ ethash_rol_u64(Bi, 1);
        Di = Be ^ ethash_rol_u64(Bo, 1);
        Do = Bi ^ ethash_rol_u64(Bu, 1);
        Du = Bo ^ ethash_rol_u64(Ba, 1);

        Ba = Aba ^ Da;
        Be = ethash_rol_u64(Age ^ De, 44);
        Bi = ethash_rol_u64(Aki ^ Di, 43);
        Bo = ethash_rol_u64(Amo ^ Do, 21);
        Bu = ethash_rol_u64(Asu ^ Du, 14);
        Eba = Ba ^ (~Be & Bi) ^ ethash_round_constants[n];
        Ebe = Be ^ (~Bi & Bo);
        Ebi = Bi ^ (~Bo & Bu);
        Ebo = Bo ^ (~Bu & Ba);
        Ebu = Bu ^ (~Ba & Be);

        Ba = ethash_rol_u64(Abo ^ Do, 28);
        Be = ethash_rol_u64(Agu ^ Du, 20);
        Bi = ethash_rol_u64(Aka ^ Da, 3);
        Bo = ethash_rol_u64(Ame ^ De, 45);
        Bu = ethash_rol_u64(Asi ^ Di, 61);
        Ega = Ba ^ (~Be & Bi);
        Ege = Be ^ (~Bi & Bo);
        Egi = Bi ^ (~Bo & Bu);
        Ego = Bo ^ (~Bu & Ba);
        Egu = Bu ^ (~Ba & Be);

        Ba = ethash_rol_u64(Abe ^ De, 1);
        Be = ethash_rol_u64(Agi ^ Di, 6);
        Bi = ethash_rol_u64(Ako ^ Do, 25);
        Bo = ethash_rol_u64(Amu ^ Du, 8);
        Bu = ethash_rol_u64(Asa ^ Da, 18);
        Eka = Ba ^ (~Be & Bi);
        Eke = Be ^ (~Bi & Bo);
        Eki = Bi ^ (~Bo & Bu);
        Eko = Bo ^ (~Bu & Ba);
        Eku = Bu ^ (~Ba & Be);

        Ba = ethash_rol_u64(Abu ^ Du, 27);
        Be = ethash_rol_u64(Aga ^ Da, 36);
        Bi = ethash_rol_u64(Ake ^ De, 10);
        Bo = ethash_rol_u64(Ami ^ Di, 15);
        Bu = ethash_rol_u64(Aso ^ Do, 56);
        Ema = Ba ^ (~Be & Bi);
        Eme = Be ^ (~Bi & Bo);
        Emi = Bi ^ (~Bo & Bu);
        Emo = Bo ^ (~Bu & Ba);
        Emu = Bu ^ (~Ba & Be);

        Ba = ethash_rol_u64(Abi ^ Di, 62);
        Be = ethash_rol_u64(Ago ^ Do, 55);
        Bi = ethash_rol_u64(Aku ^ Du, 39);
        Bo = ethash_rol_u64(Ama ^ Da, 41);
        Bu = ethash_rol_u64(Ase ^ De, 2);
        Esa = Ba ^ (~Be & Bi);
        Ese = Be ^ (~Bi & Bo);
        Esi = Bi ^ (~Bo & Bu);
        Eso = Bo ^ (~Bu & Ba);
        Esu = Bu ^ (~Ba & Be);


        // Round (n + 1): Exx -> Axx

        Ba = Eba ^ Eka ^ Ega ^ Ema ^ Esa;
        Be = Ebe ^ Eke ^ Ege ^ Eme ^ Ese;
        Bi = Ebi ^ Eki ^ Egi ^ Emi ^ Esi;
        Bo = Ebo ^ Eko ^ Ego ^ Emo ^ Eso;
        Bu = Ebu ^ Eku ^ Egu ^ Emu ^ Esu;

        Da = Bu ^ ethash_rol_u64(Be, 1);
        De = Ba ^ ethash_rol_u64(Bi, 1);
        Di = Be ^ ethash_rol_u64(Bo, 1);
        Do = Bi ^ ethash_rol_u64(Bu, 1);
        Du = Bo ^ ethash_rol_u64(Ba, 1);

        Ba = Eba ^ Da;
        Be = ethash_rol_u64(Ege ^ De, 44);
        Bi = ethash_rol_u64(Eki ^ Di, 43);
        Bo = ethash_rol_u64(Emo ^ Do, 21);
        Bu = ethash_rol_u64(Esu ^ Du, 14);
        Aba = Ba ^ (~Be & Bi) ^ ethash_round_constants[n + 1];
        Abe = Be ^ (~Bi & Bo);
        Abi = Bi ^ (~Bo & Bu);
        Abo = Bo ^ (~Bu & Ba);
        Abu = Bu ^ (~Ba & Be);

        Ba = ethash_rol_u64(Ebo ^ Do, 28);
        Be = ethash_rol_u64(Egu ^ Du, 20);
        Bi = ethash_rol_u64(Eka ^ Da, 3);
        Bo = ethash_rol_u64(Eme ^ De, 45);
        Bu = ethash_rol_u64(Esi ^ Di, 61);
        Aga = Ba ^ (~Be & Bi);
        Age = Be ^ (~Bi & Bo);
        Agi = Bi ^ (~Bo & Bu);
        Ago = Bo ^ (~Bu & Ba);
        Agu = Bu ^ (~Ba & Be);

        Ba = ethash_rol_u64(Ebe ^ De, 1);
        Be = ethash_rol_u64(Egi ^ Di, 6);
        Bi = ethash_rol_u64(Eko ^ Do, 25);
        Bo = ethash_rol_u64(Emu ^ Du, 8);
        Bu = ethash_rol_u64(Esa ^ Da, 18);
        Aka = Ba ^ (~Be & Bi);
        Ake = Be ^ (~Bi & Bo);
        Aki = Bi ^ (~Bo & Bu);
        Ako = Bo ^ (~Bu & Ba);
        Aku = Bu ^ (~Ba & Be);

        Ba = ethash_rol_u64(Ebu ^ Du, 27);
        Be = ethash_rol_u64(Ega ^ Da, 36);
        Bi = ethash_rol_u64(Eke ^ De, 10);
        Bo = ethash_rol_u64(Emi ^ Di, 15);
        Bu = ethash_rol_u64(Eso ^ Do, 56);
        Ama = Ba ^ (~Be & Bi);
        Ame = Be ^ (~Bi & Bo);
        Ami = Bi ^ (~Bo & Bu);
        Amo = Bo ^ (~Bu & Ba);
        Amu = Bu ^ (~Ba & Be);

        Ba = ethash_rol_u64(Ebi ^ Di, 62);
        Be = ethash_rol_u64(Ego ^ Do, 55);
        Bi = ethash_rol_u64(Eku ^ Du, 39);
        Bo = ethash_rol_u64(Ema ^ Da, 41);
        Bu = ethash_rol_u64(Ese ^ De, 2);
        Asa = Ba ^ (~Be & Bi);
        Ase = Be ^ (~Bi & Bo);
        Asi = Bi ^ (~Bo & Bu);
        Aso = Bo ^ (~Bu & Ba);
        Asu = Bu ^ (~Ba & Be);
    }

    state[0] = Aba;
    state[1] = Abe;
    state[2] = Abi;
    state[3] = Abo;
    state[4] = Abu;
    state[5] = Aga;
    state[6] = Age;
    state[7] = Agi;
    state[8] = Ago;
    state[9] = Agu;
    state[10] = Aka;
    state[11] = Ake;
    state[12] = Aki;
    state[13] = Ako;
    state[14] = Aku;
    state[15] = Ama;
    state[16] = Ame;
    state[17] = Ami;
    state[18] = Amo;
    state[19] = Amu;
    state[20] = Asa;
    state[21] = Ase;
    state[22] = Asi;
    state[23] = Aso;
    state[24] = Asu;
}

// The sponge-based Keccak with selectable bit size (256/512).
static inline ALWAYS_INLINE void ethash_keccak_sponge(
    uint64_t* out, size_t bits, const uint8_t* data, size_t size)
{
    static const size_t word_size = sizeof(uint64_t);
    const size_t hash_size = bits / 8;
    const size_t block_size = (1600 - bits * 2) / 8;

    size_t i;
    uint64_t* state_iter;
    uint64_t last_word = 0;
    uint8_t* last_word_iter = (uint8_t*)&last_word;

    uint64_t state[25] = {0};

    while (size >= block_size)
    {
        for (i = 0; i < (block_size / word_size); ++i)
        {
            state[i] ^= ethash_load_le_u64(data);
            data += word_size;
        }

        ethash_keccakf1600_implementation(state);

        size -= block_size;
    }

    state_iter = state;

    while (size >= word_size)
    {
        *state_iter ^= ethash_load_le_u64(data);
        ++state_iter;
        data += word_size;
        size -= word_size;
    }

    while (size > 0)
    {
        *last_word_iter = *data;
        ++last_word_iter;
        ++data;
        --size;
    }
    *last_word_iter = 0x01;
    *state_iter ^= ethash_to_le64(last_word);

    state[(block_size / word_size) - 1] ^= 0x8000000000000000ULL;

    ethash_keccakf1600_implementation(state);

    for (i = 0; i < (hash_size / word_size); ++i)
        out[i] = ethash_to_le64(state[i]);
}

// Public API: header-only static inline definitions.
static inline union ethash_hash256 ethash_keccak256(const uint8_t* data, size_t size) noexcept
{
    union ethash_hash256 hash;
    ethash_keccak_sponge(hash.word64s, 256, data, size);
    return hash;
}

static inline union ethash_hash256 ethash_keccak256_32(const uint8_t data[32]) noexcept
{
    union ethash_hash256 hash;
    ethash_keccak_sponge(hash.word64s, 256, data, 32);
    return hash;
}

static inline union ethash_hash512 ethash_keccak512(const uint8_t* data, size_t size) noexcept
{
    union ethash_hash512 hash;
    ethash_keccak_sponge(hash.word64s, 512, data, size);
    return hash;
}

static inline union ethash_hash512 ethash_keccak512_64(const uint8_t data[64]) noexcept
{
    union ethash_hash512 hash;
    ethash_keccak_sponge(hash.word64s, 512, data, 64);
    return hash;
}

#ifdef __cplusplus
}
#endif
