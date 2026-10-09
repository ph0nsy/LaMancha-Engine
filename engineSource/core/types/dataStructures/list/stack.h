/*
 *  LaMancha Engine
 *  File: stack.h
 *
 *  Copyright (C) 2026 Alonso Moreno
 *
 */

#include "arrayList.h"

namespace LaMancha {
  namespace DataStructures {
    template<typename T>
    struct StackLM {
      ArrayListLM<T> values;
    };
  }
}