#include "core/pch.h"
#include "core/log.h"
#include "core/math/LaManchaMath.h"
#include "core/types/string.h"
#include "core/types/dataStructures/hashmap.h"
//#include "deps/nanobench.h"

using namespace LaMancha;

int main() {
  Math::mat2_2 a{ { {1, 2}, {3, 4} } };
  const Math::mat2_2 b{ { {4, 3}, {2, 1} } };
  Math::mat3_2 d{ { {4, 3}, {2, 1}, {5, 6} } };
  Math::mat2_3 e{ { {3, 4, 2}, {1, 5, 6} } };
  Math::mat2_3 f{ { {5, 3, 2}, {1, 4, 6} } };
  Math::mat3_3 r3{ { {5, 3, 2}, {5, 4, 4}, {6, 1, 3} } };
  Math::mat2_2 r2{ { {5, 3}, {1, 4} } };

  Logging::Log(Logging::LogLevel::Info, "\nPrint A: ");
  for (usize i = 0; i < 2; i++) {
    for (usize j = 0; j < 2; j++) {
      String256 s;
      s.set("%f ", a[i][j]);
      Logging::Log(Logging::LogLevel::Info, s.cstr());
    }
  }

  Logging::Log(Logging::LogLevel::Info, "\nPrint B: ");
  for (usize i = 0; i < 2; i++) {
    for (usize j = 0; j < 2; j++) {
      String256 s;
      s.set("%f ", b[i][j]);
      Logging::Log(Logging::LogLevel::Info, s.cstr());
    }
  }

  Math::mat2_2 c = a + b;

  Logging::Log(Logging::LogLevel::Info, "\nPlus: ");
  for (usize i = 0; i < 2; i++) {
    for (usize j = 0; j < 2; j++) {
      String256 s;
      s.set("%f ", c[i][j]);
      Logging::Log(Logging::LogLevel::Info, s.cstr());
    }
  }

  c = a - b;

  Logging::Log(Logging::LogLevel::Info, "\nMinus: ");
  for (usize i = 0; i < 2; i++) {
    for (usize j = 0; j < 2; j++) {
      String256 s;
      s.set("%f ", c[i][j]);
      Logging::Log(Logging::LogLevel::Info, s.cstr());
    }
  }

  r3 = d * e;

  Logging::Log(Logging::LogLevel::Info, "\nMultiply: ");
  for (usize i = 0; i < 2; i++) {
    for (usize j = 0; j < 2; j++) {
      String256 s;
      s.set("%f ", r3[i][j]);
      Logging::Log(Logging::LogLevel::Info, s.cstr());
    }
  }

  r2 = e * d;

  Logging::Log(Logging::LogLevel::Info, "\nDivision: ");
  for (usize i = 0; i < 2; i++) {
    for (usize j = 0; j < 2; j++) {
      String256 s;
      s.set("%f ", r2[i][j]);
      Logging::Log(Logging::LogLevel::Info, s.cstr());
    }
  }

  c += Math::customIdentity<2>();

  Logging::Log(Logging::LogLevel::Info, "\nPlus Eq: ");
  for (usize i = 0; i < 2; i++) {
    for (usize j = 0; j < 2; j++) {
      String256 s;
      s.set("%f ", c[i][j]);
      Logging::Log(Logging::LogLevel::Info, s.cstr());
    }
  }

  c -= b;

  Logging::Log(Logging::LogLevel::Info, "\nMinus Eq: ");
  for (usize i = 0; i < 2; i++) {
    for (usize j = 0; j < 2; j++) {
      String256 s;
      s.set("%f ", c[i][j]);
      Logging::Log(Logging::LogLevel::Info, s.cstr());
    }
  }

  a *= b;

  Logging::Log(Logging::LogLevel::Info, "\nMultiply Eq: ");
  for (usize i = 0; i < 2; i++) {
    for (usize j = 0; j < 2; j++) {
      String256 s;
      s.set("%f ", a[i][j]);
      Logging::Log(Logging::LogLevel::Info, s.cstr());
    }
  }

  r2 *= a;

  Logging::Log(Logging::LogLevel::Info, "\nMultiply Eq: ");
  for (usize i = 0; i < 2; i++) {
    for (usize j = 0; j < 2; j++) {
      String256 s;
      s.set("%f ", r2[i][j]);
      Logging::Log(Logging::LogLevel::Info, s.cstr());
    }
  }

  a /= b;

  Logging::Log(Logging::LogLevel::Info, "\nMultiply Eq: ");
  for (usize i = 0; i < 2; i++) {
    for (usize j = 0; j < 2; j++) {
      String256 s;
      s.set("%f ", a[i][j]);
      Logging::Log(Logging::LogLevel::Info, s.cstr());
    }
  }

  DataStructures::HashMapLM<u32, u32> map;

  // Fill to about 50% load
  for (u32 i = 0; i < 128; i++) { map.set(i, i * 2); }
  /*
  ankerl::nanobench::Bench()
    .name("get at 50% load (hit)")
    .run([&] {
    auto r = map.get(64u);
    ankerl::nanobench::doNotOptimizeAway(r);
      });

  ankerl::nanobench::Bench()
    .name("get at 50% load (miss)")
    .run([&] {
    auto r = map.get(999u);
    ankerl::nanobench::doNotOptimizeAway(r);
      });
  */

  return 0;
}