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

#include "src/extendrt/delegate/tensorrt/tensorrt_context.h"

namespace mindspore::lite {
TensorRTContext::~TensorRTContext() {
  if (network_ != nullptr) {
    network_->destroy();
    network_ = nullptr;
  }
}

bool TensorRTContext::Init() {
  network_ = runtime_->GetBuilder()->createNetworkV2(
    1U << static_cast<uint32_t>(nvinfer1::NetworkDefinitionCreationFlag::kEXPLICIT_BATCH));
  if (network_ == nullptr) {
    MS_LOG(ERROR) << "New network init failed.";
    return false;
  }
  return true;
}

void TensorRTContext::SetRuntime(TensorRTRuntime *runtime) { runtime_ = runtime; }

nvinfer1::INetworkDefinition *TensorRTContext::network() { return network_; }

void TensorRTContext::RegisterLayer(nvinfer1::ILayer *layer, const std::string &basename) {
  if (layer == nullptr) {
    MS_LOG(ERROR) << "Register null layer!";
    return;
  }
  layer->setName((basename + "_" + std::to_string(counter_++)).c_str());
}

void TensorRTContext::RegisterTensor(nvinfer1::ITensor *tensor, const std::string &basename) {
  if (tensor == nullptr) {
    MS_LOG(ERROR) << "Register null tensor!";
    return;
  }
  tensor->setName((basename + "_" + std::to_string(counter_++)).c_str());
}
}  // namespace mindspore::lite