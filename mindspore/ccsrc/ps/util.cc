/**
 * Copyright 2020 Huawei Technologies Co., Ltd
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

#include "ps/util.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include "ps/constants.h"
#include "ps/ps_context.h"
#include "utils/ms_utils.h"

namespace mindspore {
namespace ps {
int64_t Util::rank_id_ = -1;

std::unordered_map<std::string, int64_t> Util::optimizer_to_ids{
  {kApplyMomentum, 0},
  {kSparseAdam, 1},
  {kSparseLazyAdam, 2},
  {kSparseFtrl, 3},
};

std::unordered_map<int64_t, std::string> Util::id_to_optimizers{
  {0, kApplyMomentum},
  {1, kSparseAdam},
  {2, kSparseLazyAdam},
  {3, kSparseFtrl},
};

std::unordered_map<int64_t, std::string> Util::id_to_optimizer_nodes{
  {0, kApplyMomentumOp},
  {1, kSparseAdamOp},
  {2, kSparseLazyAdamOp},
  {3, kSparseFtrlOp},
};

bool Util::IsRoleOfPServer() { return PSContext::instance()->is_server(); }

bool Util::IsRoleOfScheduler() { return PSContext::instance()->is_scheduler(); }

int64_t Util::optimizer_id(std::string name) {
  if (optimizer_to_ids.count(name) > 0) {
    return optimizer_to_ids[name];
  }
  return -1;
}

std::string Util::optimizer_name(int64_t id) {
  if (id_to_optimizers.count(id) > 0) {
    return id_to_optimizers[id];
  }
  return "";
}

std::string Util::optimizer_node_name(int64_t id) {
  if (id_to_optimizer_nodes.count(id) > 0) {
    return id_to_optimizer_nodes[id];
  }
  return "";
}

bool Util::is_optimizer(std::string name) { return optimizer_to_ids.count(name) > 0; }

int64_t Util::LocalShard(int64_t first_dim, int64_t rank_id, int64_t server_num) {
  std::map<int64_t, int64_t> shard_dims = AllRankLocalShard(first_dim, rank_id, server_num);
  if (shard_dims.count(rank_id) == 0) {
    MS_LOG(EXCEPTION) << "Invalid rank id " << rank_id;
  }
  return shard_dims[rank_id];
}

std::map<int64_t, int64_t> Util::AllRankLocalShard(int64_t first_dim, int64_t rank_id, int64_t server_num) {
  if (first_dim <= 0 || server_num <= 0 || rank_id < 0) {
    MS_LOG(EXCEPTION) << "Input values are invalid.";
  }
  if (rank_id >= server_num) {
    MS_LOG(EXCEPTION) << "The rank ID " << rank_id << " should be less than the number of servers " << server_num;
  }
  std::map<int64_t, int64_t> shard_dims;
  for (int64_t i = 0; i < server_num; i++) {
    shard_dims[i] = 0;
  }
  if (server_num != static_cast<int64_t>(shard_dims.size())) {
    MS_LOG(EXCEPTION) << "Inconsistent server num " << server_num << " shard dims counter size " << shard_dims.size();
  }
  int64_t server_index = -1;
  for (int64_t i = 0; i < first_dim; i++) {
    server_index = (server_index + 1) % server_num;
    shard_dims[server_index] = shard_dims[server_index] + 1;
  }
  if (shard_dims.count(rank_id) == 0) {
    MS_LOG(EXCEPTION) << "Invalid rank id " << rank_id << ", total server num " << server_num;
  }
  return shard_dims;
}

void Util::ReduceSparseGradient(float *gradients, int *indices, const size_t indices_size, size_t segment_size,
                                const size_t first_dim_size, const size_t outer_dim_size,
                                mindspore::kernel::SparseGradient<int> *unique_sparse_grad) {
  size_t slice_segment_size = indices_size * segment_size;
  std::vector<float> workspace_grad(slice_segment_size);
  std::vector<int> workspace_indices(indices_size);

  MS_EXCEPTION_IF_NULL(gradients);
  MS_EXCEPTION_IF_NULL(indices);

  mindspore::kernel::SparseGradient<int> workspace_sparse_grad(
    {workspace_grad.data(), workspace_indices.data(), indices_size});
  mindspore::kernel::SparseGradient<int> input_sparse_grad({gradients, indices, indices_size});
  mindspore::kernel::ReduceSparseGradientParam<int> param;
  param.input_grad_ = &input_sparse_grad;
  param.workspace_grad_ = &workspace_sparse_grad;
  param.output_grad_ = unique_sparse_grad;
  param.max_index_ = first_dim_size;
  param.value_stride_ = outer_dim_size;

  mindspore::kernel::SparseOptimizerCPUKernel::BucketReduceSparseGradient(param);
}

bool Util::FuseServerCommOps(const pipeline::ResourcePtr &res) {
  FuncGraphPtr func_graph = res->func_graph();
  MS_EXCEPTION_IF_NULL(func_graph);
  DoFusion(func_graph, kPullWeightOpName, kFusedPullWeightOpName);
  DoFusion(func_graph, kPushWeightOpName, kFusedPushWeightOpName);
  return true;
}

void Util::DoFusion(FuncGraphPtr func_graph, const std::string &cnode_name, const std::string &fused_cnode_name) {
  MS_EXCEPTION_IF_NULL(func_graph);
  std::vector<AnfNodePtr> node_list = TopoSort(func_graph->get_return());

  std::vector<AnfNodePtr> single_nodes;
  std::vector<std::string> weight_names;
  std::vector<int64_t> indices;
  for (const AnfNodePtr &node : node_list) {
    if (node != nullptr && node->isa<CNode>()) {
      if (AnfAlgo::GetCNodeName(node) == cnode_name) {
        single_nodes.push_back(node);

        auto weight_name_value_node =
          AnfAlgo::GetInputNode(node->cast<CNodePtr>(), kNodeInputWeightNameOffset)->cast<ValueNodePtr>();
        const std::string &weight_name = GetValue<std::string>(weight_name_value_node->value());
        weight_names.push_back(weight_name);

        auto weight_index_value_node =
          AnfAlgo::GetInputNode(node->cast<CNodePtr>(), kNodeInputWeightIndexOffset)->cast<ValueNodePtr>();
        int64_t weight_index = GetValue<int64_t>(weight_index_value_node->value());
        indices.push_back(weight_index);
      }
    }
  }

  auto prim = std::make_shared<Primitive>(fused_cnode_name);
  MS_EXCEPTION_IF_NULL(prim);
  std::vector<AnfNodePtr> fused_node_inputs = {};
  fused_node_inputs.push_back(NewValueNode(prim));
  std::for_each(single_nodes.begin(), single_nodes.end(), [&](AnfNodePtr node) {
    fused_node_inputs.push_back(AnfAlgo::GetInputNode(node->cast<CNodePtr>(), 0));
  });

  auto fused_cnode = func_graph->NewCNode(fused_node_inputs);
  MS_EXCEPTION_IF_NULL(fused_cnode);
  AnfAlgo::SetNodeAttr(kAttrPsKey, MakeValue(weight_names), fused_cnode);
  AnfAlgo::SetNodeAttr(kAttrIndex, MakeValue(indices), fused_cnode);
  AnfAlgo::SetNodeAttr(kAttrPrimitiveTarget, MakeValue(kCPUDevice), fused_cnode);

  auto kernel_info = std::make_shared<device::KernelInfo>();
  MS_EXCEPTION_IF_NULL(kernel_info);
  fused_cnode->set_kernel_info(kernel_info);
  auto kernel_build_info = GenerateKernelBuildInfo(single_nodes);
  AnfAlgo::SetSelectKernelBuildInfo(kernel_build_info, fused_cnode.get());

  AbstractBasePtrList abstract_list;
  for (const auto &node : single_nodes) {
    auto cnode = node->cast<CNodePtr>();
    MS_EXCEPTION_IF_NULL(cnode);
    abstract_list.push_back(cnode->abstract());
  }
  auto abstract_tuple = std::make_shared<abstract::AbstractTuple>(abstract_list);
  MS_EXCEPTION_IF_NULL(abstract_tuple);
  fused_cnode->set_abstract(abstract_tuple);

  auto manager = func_graph->manager();
  MS_EXCEPTION_IF_NULL(manager);
  for (const auto &node : single_nodes) {
    if (!manager->Replace(node, fused_cnode)) {
      MS_LOG(EXCEPTION) << "manager replace node failed";
    }
  }
  return;
}

kernel::KernelBuildInfoPtr Util::GenerateKernelBuildInfo(const std::vector<AnfNodePtr> &node_list) {
  std::vector<std::string> inputs_device_format;
  std::vector<std::string> outputs_device_format;
  std::vector<TypeId> inputs_device_type;
  std::vector<TypeId> outputs_device_type;
  std::vector<std::vector<size_t>> outputs_shape;
  kernel::KernelBuildInfo::KernelBuildInfoBuilder builder;
  for (size_t idx = 0; idx < node_list.size(); ++idx) {
    auto cnode = utils::cast<CNodePtr>(node_list[idx]);
    MS_EXCEPTION_IF_NULL(cnode);
    size_t input_num = AnfAlgo::GetInputTensorNum(cnode);
    for (size_t input_index = 0; input_index < input_num; ++input_index) {
      inputs_device_format.push_back(kOpFormat_DEFAULT);
      inputs_device_type.push_back(AnfAlgo::GetPrevNodeOutputInferDataType(cnode, input_index));
    }
    size_t output_num = AnfAlgo::GetOutputTensorNum(cnode);
    for (size_t output_index = 0; output_index < output_num; ++output_index) {
      outputs_device_format.push_back(kOpFormat_DEFAULT);
      outputs_device_type.push_back(AnfAlgo::GetOutputInferDataType(cnode, output_index));
      outputs_shape.push_back(AnfAlgo::GetOutputInferShape(cnode, output_index));
    }
  }
  builder.SetInputsFormat(inputs_device_format);
  builder.SetOutputsFormat(outputs_device_format);
  builder.SetInputsDeviceType(inputs_device_type);
  builder.SetOutputsDeviceType(outputs_device_type);
  return builder.Build();
}
}  // namespace ps
}  // namespace mindspore
