/**
 * Copyright 2021 Huawei Technologies Co., Ltd
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

#include "ps/server/kernel/round/push_weight_kernel.h"

namespace mindspore {
namespace ps {
namespace server {
namespace kernel {
void PushWeightKernel::InitKernel(size_t) {
  executor_ = &Executor::GetInstance();
  MS_EXCEPTION_IF_NULL(executor_);
  if (!executor_->initialized()) {
    MS_LOG(EXCEPTION) << "Executor must be initialized in server pipeline.";
    return;
  }
  local_rank_ = DistributedCountService::GetInstance().local_rank();
}

bool PushWeightKernel::Launch(const std::vector<AddressPtr> &inputs, const std::vector<AddressPtr> &workspace,
                              const std::vector<AddressPtr> &outputs) {
  MS_LOG(INFO) << "Launching PushWeightKernel kernel.";
  void *req_data = inputs[0]->addr;
  std::shared_ptr<FBBuilder> fbb = std::make_shared<FBBuilder>();
  if (fbb == nullptr || req_data == nullptr) {
    MS_LOG(ERROR) << "FBBuilder builder or req_data is nullptr.";
    return false;
  }

  const schema::RequestPushWeight *push_weight_req = flatbuffers::GetRoot<schema::RequestPushWeight>(req_data);
  if (push_weight_req == nullptr) {
    std::string reason = "Building flatbuffers schema failed for RequestPushWeight";
    BuildPushWeightRsp(fbb, schema::ResponseCode_RequestError, reason);
    GenerateOutput(outputs, fbb->GetBufferPointer(), fbb->GetSize());
    return false;
  }

  PushWeight(fbb, push_weight_req);
  GenerateOutput(outputs, fbb->GetBufferPointer(), fbb->GetSize());
  return true;
}

bool PushWeightKernel::Reset() {
  MS_LOG(INFO) << "PushWeightKernel reset!";
  StopTimer();
  DistributedCountService::GetInstance().ResetCounter(name_);
  return true;
}

void PushWeightKernel::OnLastCountEvent(const std::shared_ptr<core::MessageHandler> &) {
  if (PSContext::instance()->resetter_round() == ResetterRound::kPushWeight) {
    FinishIteration();
  }
  return;
}

void PushWeightKernel::PushWeight(std::shared_ptr<FBBuilder> fbb, const schema::RequestPushWeight *push_weight_req) {
  if (fbb == nullptr || push_weight_req == nullptr) {
    return;
  }
  size_t iteration = static_cast<size_t>(push_weight_req->iteration());
  if (iteration != LocalMetaStore::GetInstance().curr_iter_num()) {
    std::string reason = "PushWeight iteration number is invalid:" + std::to_string(iteration) +
                         ", current iteration:" + std::to_string(LocalMetaStore::GetInstance().curr_iter_num());
    BuildPushWeightRsp(fbb, schema::ResponseCode_OutOfTime, reason);
    MS_LOG(ERROR) << reason;
    return;
  }

  std::map<std::string, Address> upload_feature_map = ParseFeatureMap(push_weight_req);
  if (upload_feature_map.empty()) {
    std::string reason = "PushWeight overwrite feature_map is empty.";
    BuildPushWeightRsp(fbb, schema::ResponseCode_RequestError, reason);
    MS_LOG(ERROR) << reason;
    return;
  }

  if (!executor_->HandlePushWeight(upload_feature_map)) {
    std::string reason = "OverwriteWeights failed.";
    BuildPushWeightRsp(fbb, schema::ResponseCode_SystemError, reason);
    MS_LOG(ERROR) << reason;
    return;
  }

  DistributedCountService::GetInstance().Count(name_, std::to_string(local_rank_));
  BuildPushWeightRsp(fbb, schema::ResponseCode_SUCCEED, "PushWeight succeed.");
  return;
}

std::map<std::string, Address> PushWeightKernel::ParseFeatureMap(const schema::RequestPushWeight *push_weight_req) {
  RETURN_IF_NULL(push_weight_req, {});
  std::map<std::string, Address> upload_feature_map;
  auto fbs_feature_map = push_weight_req->feature_map();
  for (size_t i = 0; i < fbs_feature_map->size(); i++) {
    std::string weight_full_name = fbs_feature_map->Get(i)->weight_fullname()->str();
    float *weight_data = const_cast<float *>(fbs_feature_map->Get(i)->data()->data());
    size_t weight_size = fbs_feature_map->Get(i)->data()->size() * sizeof(float);
    upload_feature_map[weight_full_name] = {weight_data, weight_size};
  }
  return upload_feature_map;
}

void PushWeightKernel::BuildPushWeightRsp(std::shared_ptr<FBBuilder> fbb, const schema::ResponseCode retcode,
                                          const std::string &reason) {
  auto fbs_reason = fbb->CreateString(reason);
  schema::ResponsePushWeightBuilder rsp_push_weight_builder(*(fbb.get()));
  rsp_push_weight_builder.add_retcode(retcode);
  rsp_push_weight_builder.add_reason(fbs_reason);
  auto rsp_push_weight = rsp_push_weight_builder.Finish();
  fbb->Finish(rsp_push_weight);
  return;
}

REG_ROUND_KERNEL(pushWeight, PushWeightKernel)
}  // namespace kernel
}  // namespace server
}  // namespace ps
}  // namespace mindspore
