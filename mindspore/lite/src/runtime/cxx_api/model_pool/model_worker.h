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

#ifndef MINDSPORE_LITE_SRC_CXX_API_MODEL_POOL_MODEL_WORKER_H_
#define MINDSPORE_LITE_SRC_CXX_API_MODEL_POOL_MODEL_WORKER_H_
#include <queue>
#include <string>
#include <mutex>
#include <future>
#include <vector>
#include <utility>
#include <memory>
#include <map>
#include "include/api/model.h"
#include "src/runtime/cxx_api/model_pool/predict_task_queue.h"
namespace mindspore {
class PredictTaskQueue;

struct WorkerConfig {
  std::map<std::string, std::map<std::string, std::string>> config_info;
  std::shared_ptr<Context> context = nullptr;
  int numa_id = 0;
};

class ModelWorker {
 public:
  ModelWorker() = default;

  ~ModelWorker() = default;

  Status Init(const char *model_buf, size_t size, const std::shared_ptr<WorkerConfig> &worker_config);

  Status UpdateConfig(const std::string &section, const std::pair<std::string, std::string> &config);

  std::vector<MSTensor> GetInputs();

  std::vector<MSTensor> GetOutputs();

  Status Predict(const std::vector<MSTensor> &inputs, std::vector<MSTensor> *outputs,
                 const MSKernelCallBack &before = nullptr, const MSKernelCallBack &after = nullptr);

  void WaitCreateWorkerDone();

  bool IsAvailable();

  void CreateThreadWorker(const char *model_buf, size_t size, const std::shared_ptr<WorkerConfig> &worker_config,
                          const std::shared_ptr<PredictTaskQueue> &predict_task_queue, bool *create_success);

 private:
  void Run(int node_id, const std::shared_ptr<PredictTaskQueue> &predict_task_queue);

  std::pair<std::vector<std::vector<int64_t>>, bool> GetModelResize(const std::vector<MSTensor> &model_inputs,
                                                                    const std::vector<MSTensor> &inputs);
  Status ResizeInit();

  Status CopyOutputTensor(std::vector<MSTensor> model_outputs, std::vector<MSTensor> *user_outputs);

 private:
  bool need_init_resize_ = true;
  std::shared_ptr<mindspore::Model> model_ = nullptr;
  std::mutex mtx_worker_;
  bool need_copy_output_ = true;
  std::atomic_bool available_ = true;
  std::shared_ptr<PredictTaskQueue> predict_task_queue_ = nullptr;
  std::vector<MSTensor> origin_worker_inputs_;
  std::vector<MSTensor> origin_worker_outputs_;
  bool create_work_done_ = false;
  std::condition_variable create_work_done_condition_;
  std::mutex create_work_done_mutex_;
};
}  // namespace mindspore
#endif  // MINDSPORE_LITE_SRC_CXX_API_MODEL_POOL_MODEL_WORKER_H_