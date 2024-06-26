#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace needle {
namespace cpu {

#define ALIGNMENT 256
#define TILE 8
typedef float scalar_t;
const size_t ELEM_SIZE = sizeof(scalar_t);


/**
 * This is a utility structure for maintaining an array aligned to ALIGNMENT boundaries in
 * memory.  This alignment should be at least TILE * ELEM_SIZE, though we make it even larger
 * here by default.
 */
struct AlignedArray {
  AlignedArray(const size_t size) {
    int ret = posix_memalign((void**)&ptr, ALIGNMENT, size * ELEM_SIZE);
    if (ret != 0) throw std::bad_alloc();
    this->size = size;
  }
  ~AlignedArray() { free(ptr); }
  size_t ptr_as_int() {return (size_t)ptr; }
  scalar_t* ptr;
  size_t size;
};



void Fill(AlignedArray* out, scalar_t val) {
  /**
   * Fill the values of an aligned array with val
   */
  for (int i = 0; i < out->size; i++) {
    out->ptr[i] = val;
  }
}


size_t getLocation(const std::vector<int32_t> &strides, size_t offset, const std::vector<int32_t> &indices_vector) {
    size_t loc = offset;
    for (size_t i = 0; i < indices_vector.size(); i++) {
        loc += strides[i] * indices_vector[i];
    }
    return loc;
}

void updateIndices(const std::vector<int32_t> &shape, std::vector<int32_t> &indices_vector) {
    // 从最后一维开始，未达到最大值就只将该维度加1，达到就重置为0并向前移动.
    for (size_t i = indices_vector.size() - 1;; i--) {
        if (indices_vector[i] < shape[i] - 1) {
            indices_vector[i]++;
            break;
        }
        // 当所有维度都达到最大值时说明遍历已经完成
        else if (i == 0)
            break;
        else
            indices_vector[i] = 0;
    }
}

void Compact(const AlignedArray& a, AlignedArray* out, std::vector<int32_t> shape,
             std::vector<int32_t> strides, size_t offset) {
  /**
   * Compact an array in memory
   *
   * Args:
   *   a: non-compact representation of the array, given as input
   *   out: compact version of the array to be written
   *   shape: shapes of each dimension for a and out
   *   strides: strides of the *a* array (not out, which has compact strides)
   *   offset: offset of the *a* array (not out, which has zero offset, being compact)
   *
   * Returns:
   *  void (you need to modify out directly, rather than returning anything; this is true for all the
   *  function will implement here, so we won't repeat this note.)
   */
  /// BEGIN SOLUTION
  std::vector<int32_t> indices_vector(shape.size(),0);

  // out已经分配好了空间，out->size就是compact后底层存储的size
  for (size_t i = 0; i < out->size; i++) {
      size_t loc = getLocation(strides, offset, indices_vector);
      out->ptr[i] = a.ptr[loc];
      updateIndices(shape, indices_vector);
  }
  /// END SOLUTION
}

void EwiseSetitem(const AlignedArray& a, AlignedArray* out, std::vector<int32_t> shape,
                  std::vector<int32_t> strides, size_t offset) {
  /**
   * Set items in a (non-compact) array
   *
   * Args:
   *   a: _compact_ array whose items will be written to out
   *   out: non-compact array whose items are to be written
   *   shape: shapes of each dimension for a and out
   *   strides: strides of the *out* array (not a, which has compact strides)
   *   offset: offset of the *out* array (not a, which has zero offset, being compact)
   */
  /// BEGIN SOLUTION
  // 实现与compact类似，只不过根据offset和strides遍历的是out中的元素
  std::vector<int32_t> indices_vector(shape.size(),0);
  for (size_t i = 0; i < a.size; i++) {
      size_t loc = getLocation(strides, offset, indices_vector);
      out->ptr[loc] = a.ptr[i];
      updateIndices(shape, indices_vector);
  }
  /// END SOLUTION
}

void ScalarSetitem(const size_t size, scalar_t val, AlignedArray* out, std::vector<int32_t> shape,
                   std::vector<int32_t> strides, size_t offset) {
  /**
   * Set items is a (non-compact) array
   *
   * Args:
   *   size: number of elements to write in out array (note that this will note be the same as
   *         out.size, because out is a non-compact subset array);  it _will_ be the same as the
   *         product of items in shape, but convenient to just pass it here.
   *   val: scalar value to write to
   *   out: non-compact array whose items are to be written
   *   shape: shapes of each dimension of out
   *   strides: strides of the out array
   *   offset: offset of the out array
   */

  /// BEGIN SOLUTION
  std::vector<int32_t> indices_vector(shape.size(),0);
  for (size_t i = 0; i < size; i++) {
      size_t loc = getLocation(strides, offset, indices_vector);
      out->ptr[loc] = val;
      updateIndices(shape, indices_vector);
  }
  /// END SOLUTION
}

void EwiseAdd(const AlignedArray& a, const AlignedArray& b, AlignedArray* out) {
  /**
   * Set entries in out to be the sum of correspondings entires in a and b.
   */
  for (size_t i = 0; i < a.size; i++) {
    out->ptr[i] = a.ptr[i] + b.ptr[i];
  }
}

void ScalarAdd(const AlignedArray& a, scalar_t val, AlignedArray* out) {
  /**
   * Set entries in out to be the sum of corresponding entry in a plus the scalar val.
   */
  for (size_t i = 0; i < a.size; i++) {
    out->ptr[i] = a.ptr[i] + val;
  }
}


/**
 * In the code the follows, use the above template to create analogous element-wise
 * and and scalar operators for the following functions.  See the numpy backend for
 * examples of how they should work.
 *   - EwiseMul, ScalarMul
 *   - EwiseDiv, ScalarDiv
 *   - ScalarPower
 *   - EwiseMaximum, ScalarMaximum
 *   - EwiseEq, ScalarEq
 *   - EwiseGe, ScalarGe
 *   - EwiseLog
 *   - EwiseExp
 *   - EwiseTanh
 *
 * If you implement all these naively, there will be a lot of repeated code, so
 * you are welcome (but not required), to use macros or templates to define these
 * functions (however you want to do so, as long as the functions match the proper)
 * signatures above.
 */

// Element-wise multiplication
void EwiseMul(const AlignedArray& a, const AlignedArray& b, AlignedArray* out) {
  for (size_t i = 0; i < a.size; i++) {
    out->ptr[i] = a.ptr[i] * b.ptr[i];
  }
}

// Scalar multiplication
void ScalarMul(const AlignedArray& a, scalar_t val, AlignedArray* out) {
  for (size_t i = 0; i < a.size; i++) {
    out->ptr[i] = a.ptr[i] * val;
  }
}

// Element-wise division
void EwiseDiv(const AlignedArray& a, const AlignedArray& b, AlignedArray* out) {
  for (size_t i = 0; i < a.size; i++) {
    out->ptr[i] = a.ptr[i] / b.ptr[i];
  }
}

// Scalar division
void ScalarDiv(const AlignedArray& a, scalar_t val, AlignedArray* out) {
  for (size_t i = 0; i < a.size; i++) {
    out->ptr[i] = a.ptr[i] / val;
  }
}

// Scalar power
void ScalarPower(const AlignedArray& a, scalar_t val, AlignedArray* out) {
  for (size_t i = 0; i < a.size; i++) {
    out->ptr[i] = pow(a.ptr[i], val);
  }
}

// Element-wise maximum
void EwiseMaximum(const AlignedArray& a, const AlignedArray& b, AlignedArray* out) {
  for (size_t i = 0; i < a.size; i++) {
    out->ptr[i] = std::max(a.ptr[i], b.ptr[i]);
  }
}

// Scalar maximum
void ScalarMaximum(const AlignedArray& a, scalar_t val, AlignedArray* out) {
  for (size_t i = 0; i < a.size; i++) {
    out->ptr[i] = std::max(a.ptr[i], val);
  }
}

// Element-wise equality
void EwiseEq(const AlignedArray& a, const AlignedArray& b, AlignedArray* out) {
  for (size_t i = 0; i < a.size; i++) {
    out->ptr[i] = scalar_t(a.ptr[i] == b.ptr[i]);
  }
}

// Scalar equality
void ScalarEq(const AlignedArray& a, scalar_t val, AlignedArray* out) {
  for (size_t i = 0; i < a.size; i++) {
    out->ptr[i] = scalar_t(a.ptr[i] == val);
  }
}

// Element-wise greater than or equal to
void EwiseGe(const AlignedArray& a, const AlignedArray& b, AlignedArray* out) {
  for (size_t i = 0; i < a.size; i++) {
    out->ptr[i] = scalar_t(a.ptr[i] >= b.ptr[i]);
  }
}

// Scalar greater than or equal to
void ScalarGe(const AlignedArray& a, scalar_t val, AlignedArray* out) {
  for (size_t i = 0; i < a.size; i++) {
    out->ptr[i] = scalar_t(a.ptr[i] >= val);
  }
}

// Element-wise logarithm
void EwiseLog(const AlignedArray& a, AlignedArray* out) {
  for (size_t i = 0; i < a.size; i++) {
    out->ptr[i] = log(a.ptr[i]);
  }
}

// Element-wise exponential
void EwiseExp(const AlignedArray& a, AlignedArray* out) {
  for (size_t i = 0; i < a.size; i++) {
    out->ptr[i] = exp(a.ptr[i]);
  }
}

// Element-wise hyperbolic tangent
void EwiseTanh(const AlignedArray& a, AlignedArray* out) {
  for (size_t i = 0; i < a.size; i++) {
    out->ptr[i] = tanh(a.ptr[i]);
  }
}


void Matmul(const AlignedArray& a, const AlignedArray& b, AlignedArray* out, uint32_t m, uint32_t n,
            uint32_t p) {
  /**
   * Multiply two (compact) matrices into an output (also compact) matrix.  For this implementation
   * you can use the "naive" three-loop algorithm.
   *
   * Args:
   *   a: compact 2D array of size m x n
   *   b: compact 2D array of size n x p
   *   out: compact 2D array of size m x p to write the output to
   *   m: rows of a / out
   *   n: columns of a / rows of b
   *   p: columns of b / out
   */

  /// BEGIN SOLUTION
  for (uint32_t i = 0; i < m; i++) {
      for (uint32_t j = 0; j < p; j++) {
          out->ptr[i * p + j] = 0;
          for (uint32_t k = 0; k < n; k++) {
              out->ptr[i * p + j] += a.ptr[i * n + k] * b.ptr[k * p + j];
          }
      }
  }
  /// END SOLUTION
}

inline void AlignedDot(const float* __restrict__ a,
                       const float* __restrict__ b,
                       float* __restrict__ out) {

  /**
   * Multiply together two TILE x TILE matrices, and _add _the result to out (it is important to add
   * the result to the existing out, which you should not set to zero beforehand).  We are including
   * the compiler flags here that enable the compile to properly use vector operators to implement
   * this function.  Specifically, the __restrict__ keyword indicates to the compile that a, b, and
   * out don't have any overlapping memory (which is necessary in order for vector operations to be
   * equivalent to their non-vectorized counterparts (imagine what could happen otherwise if a, b,
   * and out had overlapping memory).  Similarly the __builtin_assume_aligned keyword tells the
   * compiler that the input array will be aligned to the appropriate blocks in memory, which also
   * helps the compiler vectorize the code.
   *
   * Args:
   *   a: compact 2D array of size TILE x TILE
   *   b: compact 2D array of size TILE x TILE
   *   out: compact 2D array of size TILE x TILE to write to
   */

  a = (const float*)__builtin_assume_aligned(a, TILE * ELEM_SIZE);
  b = (const float*)__builtin_assume_aligned(b, TILE * ELEM_SIZE);
  out = (float*)__builtin_assume_aligned(out, TILE * ELEM_SIZE);

  /// BEGIN SOLUTION
  for (uint32_t i = 0; i < TILE; i++) {
      for (uint32_t j = 0; j < TILE; j++) {
          // 不要将out[i * TILE + j]设置为0，因为out已经是现有的累加结果
          for (uint32_t k = 0; k < TILE; k++) {
              out[i * TILE + j] += a[i * TILE + k] * b[k * TILE + j];
          }
      }
  }
  /// END SOLUTION
}

// python3 -m pytest -v -k "matmul_tiled"
void MatmulTiled(const AlignedArray& a, const AlignedArray& b, AlignedArray* out, uint32_t m,
                 uint32_t n, uint32_t p) {
  /**
   * Matrix multiplication on tiled representations of array.  In this setting, a, b, and out
   * are all *4D* compact arrays of the appropriate size, e.g. a is an array of size
   *   a[m/TILE][n/TILE][TILE][TILE]
   * You should do the multiplication tile-by-tile to improve performance of the array (i.e., this
   * function should call `AlignedDot()` implemented above).
   *
   * Note that this function will only be called when m, n, p are all multiples of TILE, so you can
   * assume that this division happens without any remainder.
   *
   * Args:
   *   a: compact 4D array of size m/TILE x n/TILE x TILE x TILE
   *   b: compact 4D array of size n/TILE x p/TILE x TILE x TILE
   *   out: compact 4D array of size m/TILE x p/TILE x TILE x TILE to write to
   *   m: rows of a / out
   *   n: columns of a / rows of b
   *   p: columns of b / out
   *
   */
  /// BEGIN SOLUTION
  std::vector<float> tile_a(TILE * TILE);
  std::vector<float> tile_b(TILE * TILE);
  // 把原来(m, n)和(n, p)的矩阵乘变成(m / TILE, n / TILE)和(n / TILE, p / TILE)的分块矩阵乘
  // 你会发现代码实现的格式是差不多的。虽然复杂但是整个流程还是很好记的
  for (uint32_t i = 0; i < m / TILE; i++) {
      for (uint32_t j = 0; j < p / TILE; j++) {
          std::vector<float> tmp_res(TILE * TILE, 0);
          for (uint32_t k = 0; k < n / TILE; k++) {
              // make tile
              for (uint32_t ii = 0; ii < TILE; ii++) {
                  for (uint32_t jj = 0; jj < TILE; jj++) {
                      // 可以把4D array看成是每个元素为2D TILE的 2D Array
                      // 获得a[i, k]和b[k, j]对应的2D TILE
                      // 基于stride format求index
                      tile_a[ii * TILE + jj] = a.ptr[i * n * TILE + k * TILE * TILE + ii * TILE + jj];
                      tile_b[ii * TILE + jj] = b.ptr[k * p * TILE + j * TILE * TILE + ii * TILE + jj];
                  }
              }
              // tile mul
              AlignedDot(tile_a.data(), tile_b.data(), tmp_res.data());
          }
          // 把分块结果写回out[i, j]对应的2D TILE
          for (uint32_t ii = 0; ii < TILE; ii++) {
              for (uint32_t jj = 0; jj < TILE; jj++) {
                  out->ptr[i * p * TILE + j * TILE * TILE + ii * TILE + jj] = tmp_res[ii * TILE + jj];
              }
          }
      }
  }
  /// END SOLUTION
}

// Because summing over individual axes can be a bit tricky, even for compact arrays
// these functions in Python simplify things by permuting the last axis to be the one reduced over
// (this is what the reduce_view_out() function in NDArray does), then compacting the array.
// So for your ReduceMax() and ReduceSum() functions you implement in C++
// you can assume that both the input and output arrays are contiguous in memory
// and you want to just reduce over contiguous elements of size reduce_size as passed to the C++ functions.
// 已经经过了预处理，要reduce的维度总是被排列到了最后一维
void ReduceMax(const AlignedArray& a, AlignedArray* out, size_t reduce_size) {
  /**
   * Reduce by taking maximum over `reduce_size` contiguous blocks.
   *
   * Args:
   *   a: compact array of size a.size = out.size * reduce_size to reduce over
   *   out: compact array to write into
   *   reduce_size: size of the dimension to reduce over
   */

  /// BEGIN SOLUTION
  size_t cnt = 0;
  for (size_t i = 0; i < a.size; i+=reduce_size) {
      scalar_t max_num = a.ptr[i];
      for (size_t j = i + 1; j < i + reduce_size; j++) {
          max_num = std::max(max_num, a.ptr[j]);
      }
      out->ptr[cnt++] = max_num;
  }
  /// END SOLUTION
}

void ReduceSum(const AlignedArray& a, AlignedArray* out, size_t reduce_size) {
  /**
   * Reduce by taking sum over `reduce_size` contiguous blocks.
   *
   * Args:
   *   a: compact array of size a.size = out.size * reduce_size to reduce over
   *   out: compact array to write into
   *   reduce_size: size of the dimension to reduce over
   */

  /// BEGIN SOLUTION
  size_t cnt = 0;
  for (size_t i = 0; i < a.size; i+=reduce_size) {
      scalar_t sum_res = 0;
      for (size_t j = i; j < i + reduce_size; j++) {
          sum_res += a.ptr[j];
      }
      out->ptr[cnt++] = sum_res;
  }
  /// END SOLUTION
}

}  // namespace cpu
}  // namespace needle

PYBIND11_MODULE(ndarray_backend_cpu, m) {
  namespace py = pybind11;
  using namespace needle;
  using namespace cpu;

  m.attr("__device_name__") = "cpu";
  m.attr("__tile_size__") = TILE;

  py::class_<AlignedArray>(m, "Array")
      .def(py::init<size_t>(), py::return_value_policy::take_ownership)
      .def("ptr", &AlignedArray::ptr_as_int)
      .def_readonly("size", &AlignedArray::size);

  // return numpy array (with copying for simplicity, otherwise garbage
  // collection is a pain)
  m.def("to_numpy", [](const AlignedArray& a, std::vector<size_t> shape,
                       std::vector<size_t> strides, size_t offset) {
    std::vector<size_t> numpy_strides = strides;
    std::transform(numpy_strides.begin(), numpy_strides.end(), numpy_strides.begin(),
                   [](size_t& c) { return c * ELEM_SIZE; });
    return py::array_t<scalar_t>(shape, numpy_strides, a.ptr + offset);
  });

  // convert from numpy (with copying)
  m.def("from_numpy", [](py::array_t<scalar_t> a, AlignedArray* out) {
    std::memcpy(out->ptr, a.request().ptr, out->size * ELEM_SIZE);
  });

  m.def("fill", Fill);
  m.def("compact", Compact);
  m.def("ewise_setitem", EwiseSetitem);
  m.def("scalar_setitem", ScalarSetitem);
  m.def("ewise_add", EwiseAdd);
  m.def("scalar_add", ScalarAdd);

   m.def("ewise_mul", EwiseMul);
   m.def("scalar_mul", ScalarMul);
   m.def("ewise_div", EwiseDiv);
   m.def("scalar_div", ScalarDiv);
   m.def("scalar_power", ScalarPower);

   m.def("ewise_maximum", EwiseMaximum);
   m.def("scalar_maximum", ScalarMaximum);
   m.def("ewise_eq", EwiseEq);
   m.def("scalar_eq", ScalarEq);
   m.def("ewise_ge", EwiseGe);
   m.def("scalar_ge", ScalarGe);

   m.def("ewise_log", EwiseLog);
   m.def("ewise_exp", EwiseExp);
   m.def("ewise_tanh", EwiseTanh);

   m.def("matmul", Matmul);
   m.def("matmul_tiled", MatmulTiled);

   m.def("reduce_max", ReduceMax);
   m.def("reduce_sum", ReduceSum);
}
