/**
 * Copyright 2022 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <thread>
#include <vector>
#include <algorithm>
#include <map>
#include "plugin/device/cpu/kernel/apply_adagrad_da_cpu_kernel.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kSizeFloat16 = 2;
constexpr size_t kSizeFloat32 = 4;
constexpr size_t kSizeInt32 = 4;
constexpr size_t kSizeInt64 = 8;
constexpr size_t kApplyAdagradDAInputsNum = 8;
constexpr size_t kApplyAdagradDAOutputsNum = 3;
constexpr size_t kVarIndex = 0;
constexpr size_t kAccIndex = 1;
constexpr size_t kSquarAccIndex = 2;
constexpr size_t kGradIndex = 3;
constexpr size_t kLRIndex = 4;
constexpr size_t kL1Index = 5;
constexpr size_t kL2Index = 6;
constexpr size_t kStepIndex = 7;
}  // namespace

bool ApplyAdagradDACpuKernelMod::Init(const BaseOperatorPtr &base_operator, const std::vector<KernelTensorPtr> &inputs,
                                      const std::vector<KernelTensorPtr> &outputs) {
  kernel_name_ = base_operator->name();
  dtype_ = inputs[0]->GetDtype();
  return true;
}

int ApplyAdagradDACpuKernelMod::Resize(const BaseOperatorPtr &base_operator, const std::vector<KernelTensorPtr> &inputs,
                                       const std::vector<KernelTensorPtr> &outputs,
                                       const std::map<uint32_t, tensor::TensorPtr> &inputsOnHost) {
  int ret = KernelMod::Resize(base_operator, inputs, outputs, inputsOnHost);
  if (ret != 0) {
    return ret;
  }
  return 0;
}

bool ApplyAdagradDACpuKernelMod::Launch(const std::vector<AddressPtr> &inputs, const std::vector<AddressPtr> &,
                                        const std::vector<AddressPtr> &outputs) {
  CheckParam(inputs, outputs);
  if (dtype_ == kNumberTypeFloat16) {
    LaunchKernel<float16>(inputs, outputs);
  } else if (dtype_ == kNumberTypeFloat32) {
    LaunchKernel<float>(inputs, outputs);
  } else {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the dtype of 'var' must be Float16 or Float32, but got "
                      << TypeIdToType(dtype_)->ToString();
  }
  return true;
}

void ApplyAdagradDACpuKernelMod::CheckShapeAndDtypeEqual(int64_t size_a, int64_t size_b, const char *name_a,
                                                         const char *name_b) {
  if (size_a != size_b) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the shape and dtype of '" << name_a << "' and '" << name_b
                      << "' must be the same, "
                         "but got the memory size of '"
                      << name_a << "': " << size_a << " and '" << name_b << "': " << size_b;
  }
}

template <typename T>
void ApplyAdagradDACpuKernelMod::CheckTypeSize(T inputs, int64_t input_size, const char *input_name,
                                               const int64_t desired_type_a_size, const char *type_a_name,
                                               const int64_t desired_type_b_size, const char *type_b_name) {
  if (input_size != desired_type_a_size && input_size != desired_type_b_size) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the '" << input_name << "' must be " << type_a_name
                      << "(memory size: " << desired_type_a_size << ") or " << type_b_name
                      << "(memory size:" << desired_type_b_size << "), but got '" << input_name << "': " << inputs
                      << ", with memory size: " << input_size << " bytes.";
  }
}

void ApplyAdagradDACpuKernelMod::CheckParam(const std::vector<AddressPtr> &inputs,
                                            const std::vector<AddressPtr> &outputs) {
  // inputs: var, gradient_accumulator, gradient_squared_accumulator, grad, lr, l1, l2, global_step
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kApplyAdagradDAInputsNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kApplyAdagradDAOutputsNum, kernel_name_);
  CheckShapeAndDtypeEqual(inputs[kAccIndex]->size, inputs[kVarIndex]->size, "gradient_accumulator", "var");
  CheckShapeAndDtypeEqual(inputs[kSquarAccIndex]->size, inputs[kVarIndex]->size, "gradient_squared_accumulator", "var");
  CheckShapeAndDtypeEqual(inputs[kGradIndex]->size, inputs[kVarIndex]->size, "grad", "var");
  CheckTypeSize<float>(reinterpret_cast<float *>(inputs[kLRIndex]->addr)[0], inputs[kLRIndex]->size, "lr", kSizeFloat16,
                       "float16", kSizeFloat32, "float32");
  CheckTypeSize<float>(reinterpret_cast<float *>(inputs[kL1Index]->addr)[0], inputs[kL1Index]->size, "l1", kSizeFloat16,
                       "float16", kSizeFloat32, "float32");
  CheckTypeSize<float>(reinterpret_cast<float *>(inputs[kL2Index]->addr)[0], inputs[kL2Index]->size, "l2", kSizeFloat16,
                       "float16", kSizeFloat32, "float32");
  CheckTypeSize<int>(reinterpret_cast<int *>(inputs[kStepIndex]->addr)[0], inputs[kStepIndex]->size, "global_step",
                     kSizeInt32, "int32", kSizeInt64, "int64");
}

template <typename T>
void ApplyAdagradDACpuKernelMod::LaunchKernel(const std::vector<AddressPtr> &inputs,
                                              const std::vector<AddressPtr> &outputs) {
  auto *var = reinterpret_cast<T *>(inputs[kVarIndex]->addr);
  auto *gradient_accumulator = reinterpret_cast<T *>(inputs[kAccIndex]->addr);
  auto *gradient_squared_accumulator = reinterpret_cast<T *>(inputs[kSquarAccIndex]->addr);
  const auto *grad = reinterpret_cast<T *>(inputs[kGradIndex]->addr);
  const auto *lr = reinterpret_cast<T *>(inputs[kLRIndex]->addr);
  const auto *l1 = reinterpret_cast<T *>(inputs[kL1Index]->addr);
  const auto *l2 = reinterpret_cast<T *>(inputs[kL2Index]->addr);
  const int *global_step = reinterpret_cast<int *>(inputs[kStepIndex]->addr);

  // multithreading
  size_t length = inputs[kVarIndex]->size / sizeof(T);
  auto task = [this, &var, &gradient_accumulator, &gradient_squared_accumulator, &grad, &lr, &l1, &l2, &global_step](
                size_t start, size_t end) {
    LaunchApplyAdagradDA(var, gradient_accumulator, gradient_squared_accumulator, grad, lr, l1, l2, global_step, start,
                         end);
  };
  CPUKernelUtils::ParallelForAutoSearch(task, length, &parallel_search_info_);

  // Copy result to output tensor
  auto output_var = reinterpret_cast<T *>(outputs[kVarIndex]->addr);
  auto output_gradient_accumulator = reinterpret_cast<T *>(outputs[kAccIndex]->addr);
  auto output_gradient_squared_accumulator = reinterpret_cast<T *>(outputs[kSquarAccIndex]->addr);
  auto ret = memcpy_s(output_var, outputs[kVarIndex]->size, var, inputs[kVarIndex]->size);
  if (ret != EOK) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', launch kernel error: memcpy failed. Error no: " << ret;
  }
  ret = memcpy_s(output_gradient_accumulator, outputs[kAccIndex]->size, gradient_accumulator, inputs[kAccIndex]->size);
  if (ret != EOK) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', launch kernel error: memcpy failed. Error no: " << ret;
  }
  ret = memcpy_s(output_gradient_squared_accumulator, outputs[kSquarAccIndex]->size, gradient_squared_accumulator,
                 inputs[kSquarAccIndex]->size);
  if (ret != EOK) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', launch kernel error: memcpy failed. Error no: " << ret;
  }
}

template <typename T>
void ApplyAdagradDACpuKernelMod::LaunchApplyAdagradDA(T *var, T *gradient_accumulator, T *gradient_squared_accumulator,
                                                      const T *grad, const T *lr, const T *l1, const T *l2,
                                                      const int *global_step, size_t start, size_t end) const {
  for (size_t i = start; i < end; i++) {
    gradient_accumulator[i] += grad[i];
    gradient_squared_accumulator[i] += grad[i] * grad[i];
    auto minus_one = static_cast<T>(-1);
    auto zeros = static_cast<T>(0);
    auto tmp_val =
      l1[0] > zeros ? static_cast<T>(Sign(static_cast<float>(gradient_accumulator[i]))) *
                        static_cast<T>(max(
                          static_cast<float16>(abs(static_cast<float16>(gradient_accumulator[i]) -
                                                   static_cast<float16>(l1[0]) * static_cast<float16>(global_step[0]))),
                          static_cast<float16>(zeros)))
                    : gradient_accumulator[i];
    auto x_value = minus_one * lr[0] * tmp_val;
    auto y_value = static_cast<float16>(l2[0]) * static_cast<float16>(global_step[0]) * static_cast<float16>(lr[0]) +
                   static_cast<float16>(sqrt(gradient_squared_accumulator[i]));
    // update var
    var[i] = static_cast<T>(x_value) / static_cast<T>(y_value);
  }
}

std::vector<KernelAttr> ApplyAdagradDACpuKernelMod::GetOpSupport() {
  static std::vector<KernelAttr> kernel_attr_list = {KernelAttr()
                                                       .AddInputAttr(kNumberTypeFloat32)
                                                       .AddInputAttr(kNumberTypeFloat32)
                                                       .AddInputAttr(kNumberTypeFloat32)
                                                       .AddInputAttr(kNumberTypeFloat32)
                                                       .AddInputAttr(kNumberTypeFloat32)
                                                       .AddInputAttr(kNumberTypeFloat32)
                                                       .AddInputAttr(kNumberTypeFloat32)
                                                       .AddInputAttr(kNumberTypeInt32)
                                                       .AddOutputAttr(kNumberTypeFloat32)
                                                       .AddOutputAttr(kNumberTypeFloat32)
                                                       .AddOutputAttr(kNumberTypeFloat32),
                                                     KernelAttr()
                                                       .AddInputAttr(kNumberTypeFloat16)
                                                       .AddInputAttr(kNumberTypeFloat16)
                                                       .AddInputAttr(kNumberTypeFloat16)
                                                       .AddInputAttr(kNumberTypeFloat16)
                                                       .AddInputAttr(kNumberTypeFloat16)
                                                       .AddInputAttr(kNumberTypeFloat16)
                                                       .AddInputAttr(kNumberTypeFloat16)
                                                       .AddInputAttr(kNumberTypeInt64)
                                                       .AddOutputAttr(kNumberTypeFloat16)
                                                       .AddOutputAttr(kNumberTypeFloat16)
                                                       .AddOutputAttr(kNumberTypeFloat16)};
  return kernel_attr_list;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, ApplyAdagradDA, ApplyAdagradDACpuKernelMod);
}  // namespace kernel
}  // namespace mindspore