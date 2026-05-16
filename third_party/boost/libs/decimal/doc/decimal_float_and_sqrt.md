# Decimal vs Binary Float: Representation, Bit Layout, and Sqrt Optimization

## 1. Numeric meaning

| | Binary (float/double) | Decimal (decimal32/64/128) |
|---|------------------------|----------------------------|
| **Formula** | \((-1)^s \times 1.m \times 2^e\) | \((-1)^s \times \mathit{sig} \times 10^e\) |
| **Significand** | Binary fraction \(m\) (bit sequence) | Decimal integer \(\mathit{sig}\) (e.g. 1–9999999) |
| **Radix** | 2 | 10 |
| **Single value** | Unique representation | Multiple representations (cohorts, e.g. 1e5 = 10e4) |
| **Hardware sqrt** | Yes | No |

```
Binary:   value = sign × (1.xxxx₂) × 2^exp
Decimal:  value = sign × (integer sig) × 10^exp
```

---

## 2. Bit layout

### Binary32 (float) — 32 bits

```
 31  30────22  21────────────────────0
 ┌───┬─────────┬──────────────────────┐
 │ s │  exp    │   mantissa (1.m)     │
 │ 1 │   8     │        23            │
 └───┴─────────┴──────────────────────┘
 sign  biased exp   binary fraction
```

### Decimal32 (BID) — 32 bits (Boost.Decimal internal `bits_`)

```
 31  30-29  28────21  20─────────────────0
 ┌───┬──────┬─────────┬──────────────────┐
 │ s │ comb │  exp    │  significand     │
 │ 1 │  2   │ 6 or 8  │  21 or 23        │
 └───┴──────┴─────────┴──────────────────┘
 sign  comb field  decimal biased exp  integer significand (stored in binary)
```

- **Combination field**: Encodes whether exponent uses 6 or 8 bits and significand 21 or 23 bits; semantics remain "decimal exponent + integer significand".

---

## 3. Examples: 0.1 and 100 — representation and bits

### 0.1: Why float/double cannot represent it exactly

0.1 = 1/10. In **binary**, 1/10 is a recurring fraction:

```
1/10 = 0.0001 1001 1001 1001 1001 …₂  (1001 repeats)
```

Binary floating-point has only a finite mantissa, so it must round; **float/double cannot represent 0.1 exactly**, only the nearest representable value.

| Type   | Stored value for 0.1 (actual)   | Hex (bits)              |
|--------|----------------------------------|-------------------------|
| float  | ≈ 0.10000000149011612            | `0x3DCCCCCD`            |
| double | ≈ 0.10000000000000000555…        | `0x3FB999999999999A`    |

Hence `0.1 + 0.2 == 0.3` can be false for float/double; decimal represents these exactly in base 10.

### 0.1 in decimal32 (exact)

0.1 = **1 × 10⁻¹** → sig=1, e=−1. decimal32 bias is 101, so stored exponent = 100.

- BID 32-bit: `bits_ = 0x00400001`
- Binary (8-bit groups):
  ```
  0000 0000  0100 0000  0000 0000  0000 0001
  └─ s=0      └─ exp=100 (0x64)    └─ sig=1
  ```
- Fields: `s=0` | 8-bit exp=100 | 23-bit sig=1 → **0x00400001** (exact 0.1)

### 100 in decimal32

100 = **1 × 10²** → sig=1, e=2. Stored exponent = 2 + 101 = 103.

- BID 32-bit: `bits_ = 0x33800001`
- Binary sketch: `0_01100111_00000000000000000000001` (s=0, exp=103, sig=1)

---

## 4. Summary comparison

```
         Binary float              Decimal (BID)
         ─────────────              ─────────────
Storage  sign + exp + mantissa      sign + comb + exp + significand
Sig      binary fraction (1.m)      integer sig (binary encoding)
Exp      2^exp                      10^exp
sqrt     hardware or iterate on     software, sqrt of integer sig
         binary mantissa
```

---

## 5. Sqrt Implementation: SoftFloat-Inspired Design

### 5.1 Core Algorithm

Both SoftFloat (binary) and Boost.Decimal (decimal) use the same fundamental approach: normalize, table lookup, Newton-Raphson refinement, and rescale. The key difference is **radix**: binary uses 2^e, decimal uses 10^e. **All arithmetic is integer** — no floating-point rounding in the main path. See Section 8 for detailed algorithm flow.

### 5.2 Shared 90-Entry Table

`sqrt_lookup.hpp` provides:

```cpp
// k0[i] = 10^16 / sqrt(1 + i * 0.1)  at left edge of bin
// k1[i] = k0[i] - k0[i+1]  (slope for linear interpolation)
recip_sqrt_k0s[90], recip_sqrt_k1s[90]
```

Index: `index = floor((x - 1) * 10)` where x ∈ [1, 10).  
Interpolation: `r0 = k0[index] - (k1[index] * eps16 >> 16)` gives ~10 bits initial precision.

### 5.3 approx_recip_sqrt32 / approx_recip_sqrt64

`approx_recip_sqrt_impl.hpp` implements integer-only 1/√x:

| Function | Input sig range | Output scale | Newton iters | Precision |
|----------|-----------------|--------------|--------------|-----------|
| approx_recip_sqrt32 | [10⁶, 10⁷) | 10⁷/sqrt(x) | 2 | ~24 bits |
| approx_recip_sqrt64 | [10¹⁵, 10¹⁶) | 10¹⁶/sqrt(x) | 3 | ~48 bits |

- **approx_recip_sqrt32**: Working scale R ≈ 10⁵/sqrt(x) so sig×R² ≤ 10¹⁷ (fits uint64).  
  σ = 10¹⁶ − sig×R², correction = R×σ/(2×10¹⁶).
- **approx_recip_sqrt64**: y = (sig/10⁸)×(r0²/10²⁴), target 10¹⁵, σ = 10¹⁵−y, δr = r0×σ/(2×10¹⁵).

---

## 6. Integer Arithmetic Implementation

### 6.1 decimal32 (sqrt32_impl.hpp)

- sig_gx = gx × 10⁶, scale6=10⁶, scale7=10⁷
- r_scaled = approx_recip_sqrt32(sig_gx) ≈ 10⁷/sqrt(gx)
- sig_z = sig_gx × r_scaled / 10⁷ (initial sqrt(gx)×10⁶)
- Precompute target = sig_gx×10⁶ for Newton and rounding
- **1 Newton** correction: rem = target − sig_z², correction = rem/(2×sig_z)
- Final rounding: if rem < 0, --sig_z

```cpp
std::uint64_t product = static_cast<std::uint64_t>(sig_gx) * r_scaled;
std::uint32_t sig_z = static_cast<std::uint32_t>(product / scale7);
// Precompute target (avoids recomputing in Newton and rounding)
const std::uint64_t target = static_cast<std::uint64_t>(sig_gx) * scale6;
// Newton: rem = target - sig_z²; sig_z += rem/(2*sig_z)
T z{sig_z, -6};
```

### 6.2 decimal64 (sqrt64_impl.hpp)

- sig_gx = gx × 10¹⁵, scale15=10¹⁵, scale16=10¹⁶
- r_scaled = approx_recip_sqrt64(sig_gx) ≈ 10¹⁶/sqrt(gx)
- Precompute target = sig_gx×10¹⁵ for Newton and rounding
- Uses **int128::uint128_t** / **int128::int128_t** for portability (all platforms including MSVC 32-bit)

```cpp
int128::uint128_t product = static_cast<int128::uint128_t>(sig_gx) * r_scaled;
std::uint64_t sig_z = static_cast<std::uint64_t>(product / scale16);
// Precompute target (avoids recomputing in each Newton iteration and rounding)
const int128::uint128_t target = static_cast<int128::uint128_t>(sig_gx) * scale15;
// 2 Newton corrections: rem = target - sig_z²; correction = rem/(2*sig_z)
// Final: if rem < 0, --sig_z
T z{sig_z, -15};
```

### 6.3 decimal128 (sqrt128_impl.hpp)

- Uses **frexp10** for exact 34-digit significand (no FP loss)
- gx_sig from frexp10, scale33_128 = 10³³ (64×64→128)
- Initial r from approx_recip_sqrt64(sig_gx_approx) where sig_gx_approx = gx_sig/10¹⁸
- sig_z = mul128By64(gx_sig, r_scaled) / 10¹⁶ (SoftFloat-style 128×64→256, r_scaled is 64-bit)
- sig_z² = umul256(uint128(sig_z), uint128(sig_z)) — sig_z fits in 128 bits, avoids full u256×u256
- **3 Newton** iterations using u256 / i256_sub
- **Round-to-nearest**: ensure sig_z² ≤ target; if target − sig_z² > sig_z, increment sig_z

```cpp
// Precompute target (avoids recomputing in each Newton iteration)
const u256 target = umul256(gx_sig, scale33_128);
u256 sig_z = mul128By64(gx_sig, r_scaled) / scale16;
// Newton: sig_z < sqrt(10)*10^33 fits in 128 bits → sig_z_sq = umul256(uint128(sig_z), uint128(sig_z))
//         rem = target - sig_z_sq; sig_z += rem/(2*sig_z)
// Final: while sig_z²>target --sig_z; if target-sig_z²>sig_z ++sig_z
// Convert: z = T{sig_z_hi,-16} + T{sig_z_lo,-33}
```

| Type | Coefficient | Integer type | Newton | Final rounding |
|------|-------------|--------------|--------|----------------|
| decimal32 | 7 digits | uint64 | 1 | floor |
| decimal64 | 16 digits | int128::uint128_t (portable) | 2 | floor |
| decimal128 | 34 digits | u256, mul128By64, umul256 | 3 | round-to-nearest |

---

## 7. File Structure and Table Design

### 7.1 Implementation Files

```
include/boost/decimal/detail/
├── cmath/
│   ├── sqrt.hpp                    # Entry point, special cases, normalize, dispatch
│   └── impl/
│       ├── sqrt_lookup.hpp         # 90-entry k0/k1 tables
│       ├── approx_recip_sqrt_impl.hpp  # approx_recip_sqrt32, approx_recip_sqrt64
│       ├── sqrt32_impl.hpp         # decimal32: integer rem, 1 Newton
│       ├── sqrt64_impl.hpp         # decimal64: int128::uint128_t (portable), 2 Newton
│       └── sqrt128_impl.hpp        # decimal128: u256, frexp10, round-to-nearest
└── u256.hpp                        # mul128By64 (128×64→256), umul256 (128×128→256)
```

### 7.2 Table Design (sqrt_lookup.hpp)

- **90 entries** covering [1, 10) with step 0.1
- recip_sqrt_k0s[i] = 10¹⁶ / sqrt(1 + i×0.1)
- recip_sqrt_k1s[i] = k0[i] − k0[i+1]
- Index: approx_recip_sqrt32 uses `sig/100000 - 10`; approx_recip_sqrt64 uses `sig/100000000000000 - 10`
- Fixed-point eps: approx_recip_sqrt32 uses `(sig_in_bin * 42950U) >> 16`; approx_recip_sqrt64 uses `(sig_in_bin * 2815ULL) >> 42`

---

## 8. Algorithm Flow Summary

```
┌─────────────────────────────────────────────────────────────────────┐
│                         sqrt(x) Algorithm                          │
├─────────────────────────────────────────────────────────────────────┤
│  1. Handle special cases (NaN, 0, negative, infinity)              │
│                                                                     │
│  2. Normalize: x → gx ∈ [1, 10), track exponent e                  │
│     (in sqrt.hpp via frexp10 + T{sig,-(d-1)}; impls receive gx)     │
│                                                                     │
│  3. Get 1/sqrt(gx) approximation:                                  │
│     - approx_recip_sqrt32/64(sig_gx) or table+Newton in impl       │
│     - r_scaled ≈ 10^scale / sqrt(gx)                               │
│                                                                     │
│  4. Initial z: sig_z = sig_gx * r_scaled / scale                   │
│     (z ≈ sqrt(gx) scaled by appropriate power of 10)               │
│                                                                     │
│  5. Newton iterations (integer remainder):                         │
│     target = sig_gx * scale²  (or scale for 128)                   │
│     rem = target - sig_z²                                          │
│     correction = rem / (2 * sig_z)                                 │
│     sig_z += correction  (or -= if rem negative)                   │
│                                                                     │
│  6. Final rounding:                                                │
│     - decimal32/64: if rem < 0, --sig_z                            │
│     - decimal128: round-to-nearest (target-sig_z²>sig_z → ++sig_z)  │
│                                                                     │
│  7. Rescale: result = z * 10^(e/2) * (√10 if e is odd)             │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 9. Key Implementation Details

### 9.1 Scaling Summary

| Type | sig_gx scale | sig_z scale | target | z output |
|------|--------------|-------------|--------|----------|
| decimal32 | 10⁶ | 10⁶ | sig_gx×10⁶ | T{sig_z, -6} |
| decimal64 | 10¹⁵ | 10¹⁵ | sig_gx×10¹⁵ | T{sig_z, -15} |
| decimal128 | 10³³ | 10³³ | sig_gx×10³³ | T{hi,-16}+T{lo,-33} |

### 9.2 Newton Remainder Formula

z' = z + (x − z²)/(2z)  
In integer form: rem = target − sig_z², correction = rem/(2×sig_z), sig_z += correction.

### 9.3 decimal128 Round-to-Nearest

Ensure sig_z is closest integer to √target:  
If target − sig_z² > sig_z, then (sig_z+0.5)² < target, so round up.

---

## 10. Performance (sqrt_bench.py, baseline vs current)

| Type               | Baseline   | Current   | Speedup      |
|--------------------|------------|-----------|---------------|
| decimal32_t        | 1.49M ops/s | 11.36M ops/s | ✓ 7.60x (+660.2%) |
| decimal64_t        | 0.74M ops/s | 4.64M ops/s | ✓ 6.23x (+523.2%) |
| decimal128_t       | 0.23M ops/s | 0.93M ops/s | ✓ 3.97x (+296.6%) |
| decimal_fast32_t   | 1.71M ops/s | 9.34M ops/s | ✓ 5.46x (+446.2%) |
| decimal_fast64_t   | 0.78M ops/s | 3.69M ops/s | ✓ 4.71x (+370.7%) |
| decimal_fast128_t  | 0.22M ops/s | 0.81M ops/s | ✓ 3.66x (+265.7%) |

*Benchmark scripts `sqrt_bench.py` and `run_srqt_test.sh` are not in the main branch. To run: `git checkout d54af19 -- run_srqt_test.sh sqrt_bench.py test/benchmark_sqrt.cpp include/boost/decimal/detail/cmath/sqrt_baseline.hpp`.*

---

*References: IEEE 754-2019, SoftFloat 3e by John R. Hauser, Boost.Decimal implementation.*
