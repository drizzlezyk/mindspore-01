mindspore.nn.FractionalMaxPool2d
================================

.. py:class:: mindspore.nn.FractionalMaxPool2d(kernel_size, output_size=None, output_ratio=None, return_indices=False, _random_samples=None)

    对输入的多维数据进行二维的分数最大池化运算。

    对多个输入平面组成的输入上应用2D分数最大池化。在  :math:`(kH_{in}, kW_{in})` 区域上应用最大池化操作，由输出shape决定随机步长。对于任何输入shape，指定输出shape为 :math:`(H, W)` 。输出特征的数量等于输入平面的数量。
    在一个输入Tensor上应用2D fractional max pooling，可被视为组成一个2D平面。

    分数最大池化的详细描述在 `Fractional Max-Pooling <https://arxiv.org/pdf/1412.6071>`_ 。

    参数：
        - **kernel_size** (Union[int, tuple[int]]) - 指定池化核尺寸大小，如果为整数，则代表池化核的高和宽。如果为tuple，其值必须包含两个整数值分别表示池化核的高和宽。
        - **output_size** (Union[int, tuple[int]]) - 目标输出shape。如果是整数，则表示输出目标的高和宽。如果是tuple，其值必须包含两个整数值分别表示目标输出的高和宽。默认值是 `None` 。
        - **output_ratio** (Union[float, tuple[float]]) - 目标输出shape与输入shape的比率。通过输入shape和 `output_ratio` 确定输出shape。支持数据类型：float16、float32、double，数值介于0到1之间。默认值是 `None` 。
        - **return_indices** (bool) - 如果为 `True` ，返回分数最大池化的最大值的的索引值。默认值是 `False` 。
        - **_random_samples** (Tensor) - 3D张量，分数最大池化的随机步长。支持的数据类型：float16、float32、double。数值介于0到1之间。shape为 :math:`(N, C, 2)` 的Tensor。默认值是 `None` 。

    输入：
        - **input_x** (Tensor) - shape为 :math:`(N, C, H_{in}, W_{in})` 的Tensor。支持的数据类型，float16、float32、float64、int32和int64。

    输出：
        - **y** (Tensor) - 数据类型和输入相同，shape是 :math:`(N, C, output\underline{~}shape{H}, output\underline{~}shape{W})`。
        - **argmax** (Tensor) - 输出的索引，是一个张量。shape和输出 `y` 一致，数据类型是int64。仅当 `return_indices` 为True时，输出最大池化的索引值。

    异常：
        - **TypeError** - `input_x` 不是float16、float32、float64、int32或int64。
        - **TypeError** - `_random_samples` 不是float16、float32或float64。
        - **ValueError** - `kernel_size` 不是整数并且不是长度为2的元组。
        - **ValueError** - `output_shape` 不是整数并且不是长度为2的元组。
        - **ValueError** - `kernel_size`， `output_shape` 与-1的和大于 `input_x` 的对应维度的量。
        - **ValueError** - `_random_samples` 维度不是3。
        - **ValueError** - `output_size` 和 `output_ratio` 同时为 `None` 。
        - **ValueError** - `input_x` 和 `_random_samples` 的第一维度大小不相等。
        - **ValueError** - `input_x` 和 `_random_samples` 第二维度大小不相等。
        - **ValueError** - `_random_samples` 第三维度大小不是2。
