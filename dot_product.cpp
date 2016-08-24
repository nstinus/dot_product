#include <iostream>
#include <type_traits>
#include <cassert>
#include <cstdlib>
#include <chrono>

#if defined(__SSE4_1__)
#include <smmintrin.h>
#endif // __SSE4_1__

#if defined(__AVX__)
#include <immintrin.h>
#endif // __AVX__

using clock_ = std::chrono::high_resolution_clock;

template <typename T>
auto to_ns(T const& val)
{
  return std::chrono::duration_cast<std::chrono::nanoseconds>(val);
}


union UnitVec
{
#if defined(__AVX__)
  using intrinsic = __m256;
#elif defined(__SSE4_1__)
  using intrinsic = __m128;
#endif
  static constexpr size_t size = sizeof(intrinsic)/sizeof(float);

  float const& operator[](size_t idx) const
  {
    assert(idx < size);
    return f[idx];
  }

  float& operator[](size_t idx)
  {
    assert(idx < size);
    return f[idx];
  }

  UnitVec& operator+=(UnitVec const& rhs)
  {
#if defined(__AVX__)
    i = _mm256_add_ps(i, rhs.i);
#elif defined(__SSE__)
    i = _mm_add_ps(i, rhs.i);
#endif
    return *this;
  }

  float hadd() const
  {
#if defined(__AVX__)
    UnitVec ret;
    ret.i = _mm256_hadd_ps(this->i, this->i);
    ret.i = _mm256_hadd_ps(ret.i, ret.i);
    ret.i = _mm256_hadd_ps(ret.i, ret.i);
    return ret.f[0];
#elif defined(__SSE3__)
    UnitVec ret;
    ret.i = _mm_hadd_ps(this->i, this->i);
    ret.i = _mm_hadd_ps(ret.i, ret.i);
    return ret.f[0];
#endif
  }

  float f[size];
  intrinsic i;
} __attribute__((aligned(16)));

UnitVec operator*(UnitVec const& lhs, UnitVec const& rhs)
{
  UnitVec ret{0};
#if defined(__AVX__)
  ret.i = _mm256_mul_ps(lhs.i, rhs.i);
#else
  ret.i = _mm_mul_ps(lhs.i, rhs.i);
#endif
  return ret;
}

float dot_product__01(UnitVec const& a, UnitVec const& b)
{
  UnitVec r;
#if defined(__AVX__)
  r.i = _mm256_dp_ps(a.i, b.i, 0xf1);
  return r[0] + r[4];
#else
  r.i = _mm_dp_ps(a.i, b.i, 0xf1);
  return r[0];
#endif
}

float dot_product__00(UnitVec const& a, UnitVec const& b)
{
  float r = 0.0f;
  for (size_t i = 0; i < UnitVec::size; ++i)
  {
    r += a[i] * b[i];
  }
  return r;
}


template <size_t N>
struct StaticVec
{
  static_assert(N % UnitVec::size == 0, "N not a multiple of UnitVec::size");
  static constexpr size_t size = N;
  static constexpr size_t nb_unitvecs = N / UnitVec::size;

  float const& operator[](size_t idx) const
  {
    assert(idx < N);
    const auto i = std::div(static_cast<long long int>(idx),
                            static_cast<long long int>(UnitVec::size));
    // std::cout << __PRETTY_FUNCTION__
    //           << " idx " << idx << " n " << nb_unitvecs
    //           << " quot " << i.quot << " rem " << i.rem
    //           << std::endl;
    return data[i.quot][i.rem];
  }

  float& operator[](size_t idx)
  {
    assert(idx < N);
    const auto i = std::div(static_cast<long long int>(idx),
                            static_cast<long long int>(UnitVec::size));
    // std::cout << __PRETTY_FUNCTION__
    //           << " idx " << idx << " n " << nb_unitvecs
    //           << " quot " << i.quot << " rem " << i.rem
    //           << std::endl;
    return data[i.quot][i.rem];
  }

  UnitVec data[nb_unitvecs];
};

template <size_t N>
float dot_product__00(StaticVec<N> const& l, StaticVec<N> const& r)
{
  float ret{0};
  for (size_t i = 0; i < StaticVec<N>::nb_unitvecs; ++i)
  {
    ret += dot_product__00(l.data[i], r.data[i]);
  }
  return ret;
}

template <size_t N>
float dot_product__01(StaticVec<N> const& l, StaticVec<N> const& r)
{
  float ret{0};
  for (size_t i = 0; i < StaticVec<N>::nb_unitvecs; ++i)
  {
    ret += dot_product__01(l.data[i], r.data[i]);
  }
  return ret;
}

template <size_t N>
float dot_product__02(StaticVec<N> const& l, StaticVec<N> const& r)
{
  UnitVec acc{0};
  for (size_t i = 0; i < StaticVec<N>::nb_unitvecs; ++i)
  {
    acc += l.data[i] * r.data[i];
  }
  return acc.hadd();
}


int main()
{
  UnitVec a, b/*, r = {0}*/;

  for (size_t i = 0; i < UnitVec::size; ++i)
  {
    a.f[i] = b.f[UnitVec::size - i - 1] = i;
  }

  for (size_t i = 0; i < UnitVec::size; ++i)
  {
      std::cout << i
              << " a " << a[i]
              << " b " << b[i]
              << std::endl;
  }


  std::cout << "UnitVec size " << UnitVec::size
            << " " << dot_product__00(a, b)
            << " " << dot_product__01(a, b)
            << std::endl;

  constexpr size_t N = 1 << 13;
  StaticVec<N> u, v;
  for (size_t i = 0; i < StaticVec<N>::size; ++i)
  {
    u[i] = v[StaticVec<N>::size - i - 1] = i;
  }

  for (size_t i = 0; i < StaticVec<N>::size; ++i)
  {
      std::cout << i
              << " u " << u[i]
              << " v " << v[i]
              << std::endl;
  }

  const auto start_res00 = clock_::now();
  const auto res00 = dot_product__00(u, v);
  const auto elapsed_res00 = to_ns(clock_::now() - start_res00);

  std::cout << "StaticVec 00 size " << StaticVec<N>::size
            << " nb_unitvecs " << StaticVec<N>::nb_unitvecs
            << " " << res00
            << " elapsed " << elapsed_res00.count() << " ns"
            << std::endl;

  const auto start_res01 = clock_::now();
  const auto res01 = dot_product__01(u, v);
  const auto elapsed_res01 = to_ns(clock_::now() - start_res01);

  std::cout << "StaticVec 01 size " << StaticVec<N>::size
            << " nb_unitvecs " << StaticVec<N>::nb_unitvecs
            << " " << res01
            << " elapsed " << elapsed_res01.count() << " ns"
            << std::endl;

  const auto start_res02 = clock_::now();
  const auto res02 = dot_product__02(u, v);
  const auto elapsed_res02 = to_ns(clock_::now() - start_res02);

  std::cout << "StaticVec 02 size " << StaticVec<N>::size
            << " nb_unitvecs " << StaticVec<N>::nb_unitvecs
            << " " << res02
            << " elapsed " << elapsed_res02.count() << " ns"
            << std::endl;

  return 0;
}