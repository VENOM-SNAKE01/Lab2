#include "defuser.h"

#include <inttypes.h>
#include <stdio.h>

#define MAX_FIELDS 8

/*
 * 把 values 中的 count 个整数写成一行“空格分隔的无符号十进制整数”，存入 out。
 * 成功返回 0；count 为 0（还没有实现）、snprintf 出错或结果被截断时返回非零。
 */
static int write_answer(char *out, size_t cap, const uint32_t *values, size_t count) {
    size_t used = 0;
    for (size_t i = 0; i < count; ++i) {
        int size = snprintf(out + used, cap - used, "%s%" PRIu32, i ? " " : "", values[i]);
        if (size < 0 || (size_t)size >= cap - used) return 1;
        used += (size_t)size;
    }
    return count == 0;
}

int solve_phase_1(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 0;

    uint32_t lo  = (uint32_t)challenge;
    uint32_t hi  = (uint32_t)(challenge >> 32);
    uint32_t mid = (uint32_t)(challenge >> 16);

    uint32_t t2 = (uint32_t)(5 * (uint64_t)hi + lo) ^ UINT32_C(0x3902);
    uint32_t s0 = (uint32_t)(5 * (uint64_t)lo) ^ (mid + UINT32_C(0xcd4e));

    values[0] = (t2 + UINT32_C(0xb86)) ^ s0;
    count = 1;
    return write_answer(out, cap, values, count);
}

int solve_phase_2(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 0;

    values[0] = 3 * ((uint32_t)((challenge >> 39) & 0xff));
    for (uint32_t i = 1; i < 6; ++i) {
        uint32_t low = (uint32_t)((challenge >> (27 + 5 * (i - 1))) & 0x1f);
        values[i] = 3 * (values[i - 1] + i) - low;
    }
    count = 6;
    return write_answer(out, cap, values, count);
}

int solve_phase_3(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 0;

    uint32_t sel = (uint32_t)(challenge & 7);
    uint32_t x = (uint32_t)((challenge >> 32) ^ (challenge >> 3));
    uint32_t r;

    switch (sel) {
        case 0: r = (x ^ UINT32_C(0x602f)) + (x >> 2); break;
        case 1: r = (2 * x) ^ UINT32_C(0xc1f4); break;
        case 2: r = (4 * x) ^ UINT32_C(0xdb50); break;
        case 3: r = 5 * x + UINT32_C(0x12e4e); break;
        case 4: r = 3 * x + UINT32_C(0x5eb3); break;
        case 5: r = 3 * x + UINT32_C(0x7775); break;
        case 6: r = (x ^ UINT32_C(0x9a14)) + (x >> 1); break;
        default: r = 5 * x + UINT32_C(0x544c); break;
    }

    values[0] = sel;
    values[1] = r;
    count = 2;
    return write_answer(out, cap, values, count);
}

static uint32_t p4_walk(uint32_t a, uint32_t lo, uint32_t hi, uint32_t d) {
    uint32_t mid = lo + ((hi - lo) >> 1);
    if (mid == a) return (mid + UINT32_C(0xfa)) ^ d;
    uint32_t e = mid + d;
    if (a < mid) return (3 * p4_walk(a, lo, mid - 1, d)) ^ e ^ UINT32_C(0x1d);
    return (3 * p4_walk(a, mid + 1, hi, d)) ^ e ^ UINT32_C(1);
}

int solve_phase_4(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 0;

    uint32_t a = (uint32_t)(challenge & 0x7f);
    uint32_t d = (uint32_t)((challenge >> 32) ^ (challenge >> 7));

    values[0] = p4_walk(a, 0, UINT32_C(0x7f), d);
    count = 1;
    return write_answer(out, cap, values, count);
}

/* 0x405b60 处的 8 条链表起点，每项最多用到前 4 个节点：{value, field4} */
static const uint32_t p5_nodes[8][4][2] = {
    { { 0x724c, 0x27f }, { 0x75d7, 0xa3b }, { 0x801f, 0x38 }, { 0x9f10, 0x17d } },
    { { 0x75d7, 0xa3b }, { 0x801f, 0x38 }, { 0x9f10, 0x17d }, { 0x0647, 0xb5d } },
    { { 0x801f, 0x38 }, { 0x9f10, 0x17d }, { 0x0647, 0xb5d }, { 0xcf08, 0x351 } },
    { { 0x9f10, 0x17d }, { 0x0647, 0xb5d }, { 0xcf08, 0x351 }, { 0xb4ce, 0x66a } },
    { { 0x0647, 0xb5d }, { 0xcf08, 0x351 }, { 0xb4ce, 0x66a }, { 0x771e, 0x951 } },
    { { 0xcf08, 0x351 }, { 0xb4ce, 0x66a }, { 0x771e, 0x951 }, { 0x0f86, 0x726 } },
    { { 0xb4ce, 0x66a }, { 0x771e, 0x951 }, { 0x0f86, 0x726 }, { 0x2d45, 0x813 } },
    { { 0x771e, 0x951 }, { 0x0f86, 0x726 }, { 0x2d45, 0x813 }, { 0xccbf, 0x40a } },
};

int solve_phase_5(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 0;

    uint32_t start = (uint32_t)(challenge & 7);
    uint32_t steps = (uint32_t)((challenge >> 3) & 3);
    uint32_t v = (uint32_t)((challenge >> 6) ^ (challenge >> 32)) ^ UINT32_C(0x15d4);
    uint32_t out_val = 0;

    for (uint32_t i = 0; i <= steps; ++i) {
        v = 3 * v + p5_nodes[start][i][0];
        v ^= p5_nodes[start][i][1];
        out_val = p5_nodes[start][i][1];
    }

    values[0] = v;
    values[1] = out_val;
    count = 2;
    return write_answer(out, cap, values, count);
}

/* 0x405e00 处的 8 项函数指针表实际指向这 6 个小函数 */
static uint32_t p6_op0(uint32_t x) { return (x + UINT32_C(0x2d2e)) ^ (x >> 1); }
static uint32_t p6_op1(uint32_t x) { return (UINT32_C(8) * x + UINT32_C(0xc9c2)) ^ (UINT32_C(7) * x); }
static uint32_t p6_op2(uint32_t x) { return ((x << 1) | (x >> 31)) ^ UINT32_C(0x39b5); }
static uint32_t p6_op3(uint32_t x) { return (x >> 7) | (x << 25); }
static uint32_t p6_op4(uint32_t x) { return (x ^ UINT32_C(0x8adb)) + UINT32_C(4) * x; }
static uint32_t p6_op5(uint32_t x) { return ((x >> 1) ^ x) + UINT32_C(0x1346); }

/* 表项下标 -> 实际函数：0..7 对应 op4, op5, op5, op0, op5, op3, op2, op1 */
static uint32_t p6_call(uint32_t idx, uint32_t x) {
    switch (idx) {
        case 0: return p6_op4(x);
        case 1: return p6_op5(x);
        case 2: return p6_op5(x);
        case 3: return p6_op0(x);
        case 4: return p6_op5(x);
        case 5: return p6_op3(x);
        case 6: return p6_op2(x);
        default: return p6_op1(x);
    }
}

int solve_phase_6(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 0;

    uint32_t x0 = (uint32_t)((challenge >> 6) ^ (challenge >> 32));
    uint32_t y = p6_call((uint32_t)(challenge & 7), x0) ^ UINT32_C(0xb312);
    uint32_t r = p6_call((uint32_t)((challenge >> 3) & 7), y);

    values[0] = r + UINT32_C(0xfd35);
    count = 1;
    return write_answer(out, cap, values, count);
}

int solve_secret(uint64_t challenge, uint64_t context, char *out, size_t cap) {
    (void)challenge;
    (void)context;
    (void)out;
    (void)cap;
    // TODO（选做）: 根据 secret_gate、secret_copy 和 secret_phase 的汇编构造完整的第七行，写入 out 后返回 0。
    // 第七行不一定只有整数，可以直接用 snprintf 写入；还没完成时保持返回 1，测试会显示 not attempted。
    return 1;
}
