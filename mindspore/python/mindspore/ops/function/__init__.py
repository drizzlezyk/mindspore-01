# Copyright 2022 Huawei Technologies Co., Ltd
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ============================================================================

"""
Function operator.

A collection of function to build neural networks or to compute functions.
"""

from . import (
    array_func,
    parameter_func,
    math_func,
    nn_func,
    linalg_func,
)
from .array_func import (
    unique,
    unique_with_pad,
    eye,
    matrix_band_part,
    padding,
    fill,
    fill_,
    tile,
    size,
    ones,
    ones_like,
    shape,
    shape_,
    ger,
    dyn_shape,
    rank,
    reshape,
    reshape_,
    flatten,
    concat,
    stack,
    unstack,
    tensor_slice,
    slice,
    scalar_to_array,
    scalar_to_tensor,
    tuple_to_array,
    expand_dims,
    squeeze,
    transpose,
    scatter_nd,
    scatter_nd_add,
    scatter_nd_sub,
    scatter_nd_mul,
    scatter_nd_div,
    scatter_nd_max,
    scatter_nd_min,
    gather,
    gather_d,
    gather_elements,
    gather_nd,
    scalar_cast,
    masked_fill,
    tensor_scatter_add,
    tensor_scatter_sub,
    tensor_scatter_mul,
    unique_consecutive,
    tensor_scatter_div,
    tensor_scatter_max,
    tensor_scatter_min,
    tensor_scatter_elements,
    scatter_add,
    scatter_max,
    scatter_min,
    scatter_div,
    scatter_update,
    unsorted_segment_min,
    unsorted_segment_max,
    unsorted_segment_prod,
    nonzero,
    space_to_batch_nd,
    batch_to_space_nd,
    range,
    select,
    one_hot,
    matrix_diag,
    matrix_diag_part,
    matrix_set_diag,
    diag,
    masked_select,
    meshgrid,
    affine_grid,
    fills,
    broadcast_to,
    unsorted_segment_sum,
    adaptive_max_pool2d,
    col2im,
    split,
    index_fill,
    max,
    min,
    population_count,
)
from .parameter_func import (
    assign,
    assign_add,
    assign_sub,
    index_add,
)
from .math_func import (
    addn,
    absolute,
    abs,
    argmin,
    tensor_add,
    add,
    addcdiv,
    addcmul,
    neg_tensor,
    neg,
    tensor_lt,
    less,
    tensor_le,
    le,
    lerp,
    norm,
    round,
    tensor_gt,
    gt,
    tensor_ge,
    ge,
    tensor_sub,
    sub,
    tensor_mul,
    mul,
    tensor_div,
    div,
    tensor_floordiv,
    floor_div,
    floordiv,
    xdivy,
    tensor_pow,
    pow,
    pows,
    tensor_mod,
    floor_mod,
    floormod,
    lcm,
    tensor_exp,
    exp,
    tensor_expm1,
    expm1,
    equal,
    not_equal,
    ne,
    isfinite,
    isnan,
    isclose,
    hypot,
    same_type_shape,
    gcd,
    log,
    log_matrix_determinant,
    matrix_determinant,
    linspace,
    matrix_solve,
    maximum,
    logaddexp,
    logaddexp2,
    logit,
    std,
    ldexp,
    mv,
    inplace_add,
    inplace_sub,
    inplace_update,
    inv,
    invert,
    minimum,
    renorm,
    floor,
    logical_not,
    logical_or,
    logical_and,
    logsumexp,
    outer,
    sin,
    cos,
    tan,
    asin,
    acos,
    atan,
    sinh,
    cosh,
    tanh,
    asinh,
    acosh,
    atanh,
    atan2,
    bitwise_and,
    bitwise_or,
    bitwise_xor,
    erf,
    erfc,
    cdist,
    ceil,
    bernoulli,
    bessel_i0,
    bessel_i0e,
    bessel_j0,
    bessel_j1,
    bessel_k0,
    bessel_k0e,
    bessel_y0,
    bessel_y1,
    bessel_i1,
    bessel_i1e,
    bessel_k1,
    bessel_k1e,
    stft,
    exp2,
    deg2rad,
    isreal,
    rad2deg,
    truncate_div,
    truncate_mod,
    trunc,
    gumbel_softmax,
    matmul,
    baddbmm,
    cummin,
    cummax,
    amin,
    amax,
    mean,
    prod,
    all,
    any,
    sparse_segment_mean,
    log2,
    xlogy,
    log10,
    log1p,
    approximate_equal,
    frac,
    kron,
    rot90
)
from .nn_func import (
    adaptive_avg_pool2d,
    adaptive_max_pool3d,
    avg_pool2d,
    max_pool3d,
    celu,
    binary_cross_entropy_with_logits,
    dropout2d,
    dropout3d,
    deformable_conv2d,
    fast_gelu,
    hardshrink,
    soft_shrink,
    intopk,
    interpolate,
    kl_div,
    log_softmax,
    lrn,
    mish,
    hardswish,
    softsign,
    selu,
    pdist,
    pad,
    nll_loss,
    smooth_l1_loss,
    cross_entropy,
    grid_sample,
    ctc_greedy_decoder,
    ctc_loss,
    dropout
)
from .linalg_func import (
    svd,
)
from .sparse_func import (
    coalesce,
    coo2csr,
    coo_tensor_get_dense_shape,
    coo_tensor_get_indices,
    coo_tensor_get_values,
    csr_div,
    csr_gather,
    csr_mul,
    csr_mv,
    csr_reduce_sum,
    csr_to_coo,
    csr2coo,
    csr_tensor_get_dense_shape,
    csr_tensor_get_indices,
    csr_tensor_get_indptr,
    csr_tensor_get_values,
    dense_to_sparse_coo,
    dense_to_sparse_csr,
    make_sparse_tensor,
    make_coo_tensor,
    make_csr_tensor,
    make_row_tensor,
    row_tensor_get_values,
    row_tensor_get_indices,
    row_tensor_get_dense_shape,
    row_tensor_add,
    sparse_add,
    sparse_concat,
    csr_add,
    csr_softmax,
)
from .random_func import (
    standard_laplace,
    random_categorical,
    uniform,
    standard_normal,
    random_gamma,
    uniform_candidate_sampler,
    poisson,
)

__all__ = []
__all__.extend(array_func.__all__)
__all__.extend(parameter_func.__all__)
__all__.extend(math_func.__all__)
__all__.extend(nn_func.__all__)
__all__.extend(linalg_func.__all__)
__all__.extend(sparse_func.__all__)
__all__.extend(random_func.__all__)
__all__.sort()
