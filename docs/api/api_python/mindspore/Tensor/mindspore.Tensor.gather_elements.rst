mindspore.Tensor.gather_elements
================================

.. py:method:: mindspore.Tensor.gather_elements(dim, index)

    获取指定轴的元素。

    对于三维Tensor，输出为：

    .. code-block::

        output[i][j][k] = x[index[i][j][k]][j][k]  # if dim == 0

        output[i][j][k] = x[i][index[i][j][k]][k]  # if dim == 1

        output[i][j][k] = x[i][j][index[i][j][k]]  # if dim == 2

    `index` 与当前Tensor拥有一样的维度长度，且除 `dim` 维外其他维度一致。如果维度 `dim` 为i，当前Tensor是shape为 :math:`(z_0, z_1, ..., z_i, ..., z_{n-1})` 的n维Tensor，则 `index` 必须是shape为 :math:`(z_0, z_1, ..., y, ..., z_{n-1})` 的n维Tensor，其中 `y` 大于等于1，输出的shape与 `index` 相同。

    参数：
        - **dim** (int) - 获取元素的轴。数据类型为int32或int64。取值范围为[-self.ndim, self.ndim)。
        - **index** (Tensor) - 获取收集元素的索引。支持的数据类型包括：int32，int64。每个索引元素的取值范围为[-self.shape(dim), self.shape(dim))。

    返回：
        Tensor，shape与 `index` 相同，即其shape为 :math:`(z_0, z_1, ..., y, ..., z_{n-1})`，数据类型与 `self.dtype` 相同。

    异常：
        - **TypeError** - `dim` 或 `index` 的数据类型既不是int32，也不是int64。
        - **ValueError** - `self` 和 `index` 的维度长度不一致。
        - **ValueError** - `self` 和 `index` 除 `dim` 维外的维度不一致。
        - **ValueError** - `dim` 的值不在合理范围内。