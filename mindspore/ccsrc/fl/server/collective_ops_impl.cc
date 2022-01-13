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

#include "fl/server/collective_ops_impl.h"

namespace mindspore {
namespace fl {
namespace server {
void CollectiveOpsImpl::Initialize(const std::shared_ptr<ps::core::ServerNode> &server_node) {
  MS_EXCEPTION_IF_NULL(server_node);
  server_node_ = server_node;
  rank_id_ = server_node_->rank_id();
  server_num_ = ps::PSContext::instance()->initial_server_num();
  return;
}

template <typename T>
bool CollectiveOpsImpl::RingAllReduce(const void *sendbuff, void *recvbuff, size_t count) {
  MS_ERROR_IF_NULL_W_RET_VAL(server_node_, false);
  MS_ERROR_IF_NULL_W_RET_VAL(sendbuff, false);
  MS_ERROR_IF_NULL_W_RET_VAL(recvbuff, false);

  int ret;
  if (recvbuff != sendbuff) {
    size_t src_size = count * sizeof(T);
    size_t dst_size = count * sizeof(T);
    ret = memcpy_s(recvbuff, dst_size, sendbuff, src_size);
    if (ret != 0) {
      MS_LOG(ERROR) << "memcpy_s error, errorno(" << ret << ")";
      return false;
    }
  }

  uint32_t rank_size = server_num_;
  size_t chunk_size = count / rank_size;
  size_t remainder_size = count % rank_size;
  std::vector<size_t> chunk_sizes(rank_size, chunk_size);
  // The rest of the data should be assigned to each chunk.
  for (size_t i = 0; i < remainder_size; i++) {
    chunk_sizes[i]++;
  }
  // Store offsets to get every data chunk's address.
  std::vector<size_t> chunk_offset;
  for (size_t i = 0; i < rank_size; i++) {
    size_t ofs =
      std::accumulate(chunk_sizes.begin(), chunk_sizes.begin() + i, static_cast<size_t>(0), std::plus<size_t>());
    chunk_offset.push_back(ofs);
  }

  T *output_buff = reinterpret_cast<T *>(recvbuff);
  uint32_t send_to_rank = (rank_id_ + 1) % rank_size;
  uint32_t recv_from_rank = (rank_id_ - 1 + rank_size) % rank_size;
  MS_LOG(DEBUG) << "AllReduce count:" << count << ", rank_size:" << rank_size << ", rank_id_:" << rank_id_
                << ", chunk_size:" << chunk_size << ", remainder_size:" << remainder_size
                << ", chunk_sizes:" << chunk_sizes << ", send_to_rank:" << send_to_rank
                << ", recv_from_rank:" << recv_from_rank;

  // Ring ReduceScatter.
  MS_LOG(DEBUG) << "Start Ring ReduceScatter.";
  std::unique_ptr<T[]> tmp_recv_chunk = std::make_unique<T[]>(chunk_sizes[0]);
  MS_EXCEPTION_IF_NULL(tmp_recv_chunk);
  for (size_t i = 0; i < rank_size - 1; i++) {
    // Step 1: Async send data to next rank.
    size_t send_chunk_index = (rank_id_ - i + rank_size) % rank_size;
    T *send_chunk = output_buff + chunk_offset[send_chunk_index];
    auto send_req_id = server_node_->CollectiveSendAsync(ps::core::NodeRole::SERVER, send_to_rank, send_chunk,
                                                         chunk_sizes[send_chunk_index] * sizeof(T));
    // Step 2: Async receive data to next rank and wait until it's done.
    size_t recv_chunk_index = (rank_id_ - i - 1 + rank_size) % rank_size;
    T *recv_chunk = output_buff + chunk_offset[recv_chunk_index];
    MS_LOG(DEBUG) << "Ring ReduceScatter send_to_rank:" << send_to_rank << ", recv_from_rank:" << recv_from_rank
                  << ", send count:" << chunk_sizes[send_chunk_index]
                  << ", recv count:" << chunk_sizes[recv_chunk_index] << ", iteration:" << i;

    std::shared_ptr<std::vector<unsigned char>> recv_str;
    auto recv_req_id = server_node_->CollectiveReceiveAsync(ps::core::NodeRole::SERVER, recv_from_rank, &recv_str);
    if (!server_node_->CollectiveWait(recv_req_id, kCollectiveCommTimeout)) {
      MS_LOG(ERROR) << "CollectiveWait " << recv_req_id << " failed.";
      return false;
    }
    ret = memcpy_s(tmp_recv_chunk.get(), chunk_sizes[recv_chunk_index] * sizeof(T), recv_str->data(), recv_str->size());
    if (ret != 0) {
      MS_LOG(ERROR) << "memcpy_s error, errorno(" << ret << ")";
      return false;
    }
    // Step 3: Reduce the data so we can overlap the time cost of send.
    for (size_t j = 0; j < chunk_sizes[recv_chunk_index]; j++) {
      recv_chunk[j] += tmp_recv_chunk[j];
    }
    // Step 4: Wait until send is done.
    if (!server_node_->Wait(send_req_id, kCollectiveCommTimeout)) {
      MS_LOG(ERROR) << "CollectiveWait " << send_req_id << " failed.";
      return false;
    }
  }
  MS_LOG(DEBUG) << "End Ring ReduceScatter.";

  // Ring AllGather.
  MS_LOG(DEBUG) << "Start Ring AllGather.";
  for (size_t i = 0; i < rank_size - 1; i++) {
    size_t send_chunk_index = (rank_id_ - i + 1 + rank_size) % rank_size;
    T *send_chunk = output_buff + chunk_offset[send_chunk_index];
    auto send_req_id = server_node_->CollectiveSendAsync(ps::core::NodeRole::SERVER, send_to_rank, send_chunk,
                                                         chunk_sizes[send_chunk_index] * sizeof(T));
    size_t recv_chunk_index = (rank_id_ - i + rank_size) % rank_size;
    T *recv_chunk = output_buff + chunk_offset[recv_chunk_index];
    MS_LOG(DEBUG) << "Ring AllGather send_to_rank:" << send_to_rank << ", recv_from_rank:" << recv_from_rank
                  << ", send count:" << chunk_sizes[send_chunk_index]
                  << ", recv count:" << chunk_sizes[recv_chunk_index] << ", iteration:" << i;

    std::shared_ptr<std::vector<unsigned char>> recv_str;
    auto recv_req_id = server_node_->CollectiveReceiveAsync(ps::core::NodeRole::SERVER, recv_from_rank, &recv_str);
    if (!server_node_->CollectiveWait(recv_req_id, kCollectiveCommTimeout)) {
      MS_LOG(ERROR) << "CollectiveWait " << recv_req_id << " failed.";
      return false;
    }
    ret = memcpy_s(recv_chunk, chunk_sizes[recv_chunk_index] * sizeof(T), recv_str->data(), recv_str->size());
    if (ret != 0) {
      MS_LOG(ERROR) << "memcpy_s error, errorno(" << ret << ")";
      return false;
    }
    if (!server_node_->Wait(send_req_id, kCollectiveCommTimeout)) {
      MS_LOG(ERROR) << "CollectiveWait " << send_req_id << " failed.";
      return false;
    }
  }
  MS_LOG(DEBUG) << "End Ring AllGather.";
  return true;
}

template <typename T>
bool CollectiveOpsImpl::ReduceBroadcastAllReduce(const void *sendbuff, void *recvbuff, size_t count) {
  MS_ERROR_IF_NULL_W_RET_VAL(server_node_, false);
  MS_ERROR_IF_NULL_W_RET_VAL(recvbuff, false);
  MS_ERROR_IF_NULL_W_RET_VAL(sendbuff, false);
  uint32_t rank_size = server_num_;
  MS_LOG(DEBUG) << "Reduce Broadcast AllReduce rank_size:" << rank_size << ", rank_id_:" << rank_id_
                << ", count:" << count;

  size_t src_size = count * sizeof(T);
  size_t dst_size = count * sizeof(T);
  int ret = memcpy_s(recvbuff, dst_size, sendbuff, src_size);
  if (ret != 0) {
    MS_LOG(ERROR) << "memcpy_s error, errorno(" << ret << ")";
    return false;
  }
  T *output_buff = reinterpret_cast<T *>(recvbuff);
  // Reduce data to rank 0 process.
  MS_LOG(DEBUG) << "Start Reduce to rank 0 process.";
  if (rank_id_ == 0) {
    std::unique_ptr<T[]> tmp_recv_buff = std::make_unique<T[]>(count);
    MS_EXCEPTION_IF_NULL(tmp_recv_buff);
    for (uint32_t i = 1; i < rank_size; i++) {
      std::shared_ptr<std::vector<unsigned char>> recv_str;
      MS_LOG(DEBUG) << "Reduce rank 0 receive from rank " << i;
      auto recv_req_id1 = server_node_->CollectiveReceiveAsync(ps::core::NodeRole::SERVER, i, &recv_str);
      if (!server_node_->CollectiveWait(recv_req_id1, kCollectiveCommTimeout)) {
        MS_LOG(ERROR) << "CollectiveWait " << recv_req_id1 << " failed.";
        return false;
      }
      ret = memcpy_s(tmp_recv_buff.get(), count * sizeof(T), recv_str->data(), recv_str->size());
      if (ret != 0) {
        MS_LOG(ERROR) << "memcpy_s error, errorno(" << ret << ")";
        return false;
      }
      for (size_t j = 0; j < count; j++) {
        output_buff[j] += tmp_recv_buff[j];
      }
    }
  } else {
    MS_LOG(DEBUG) << "Reduce send data to rank 0 process.";
    auto send_req_id1 = server_node_->CollectiveSendAsync(ps::core::NodeRole::SERVER, 0, sendbuff, count * sizeof(T));
    if (!server_node_->Wait(send_req_id1, kCollectiveCommTimeout)) {
      MS_LOG(ERROR) << "CollectiveWait " << send_req_id1 << " failed.";
      return false;
    }
  }
  MS_LOG(DEBUG) << "End Reduce.";

  // Broadcast data to not 0 rank process.
  MS_LOG(DEBUG) << "Start broadcast from rank 0 to other processes.";
  if (rank_id_ == 0) {
    for (uint32_t i = 1; i < rank_size; i++) {
      MS_LOG(DEBUG) << "Broadcast data to process " << i;
      auto send_req_id2 =
        server_node_->CollectiveSendAsync(ps::core::NodeRole::SERVER, i, output_buff, count * sizeof(T));
      if (!server_node_->Wait(send_req_id2, kCollectiveCommTimeout)) {
        MS_LOG(ERROR) << "CollectiveWait " << send_req_id2 << " failed.";
        return false;
      }
    }
  } else {
    MS_LOG(DEBUG) << "Broadcast receive from rank 0.";
    std::shared_ptr<std::vector<unsigned char>> recv_str;
    auto recv_req_id2 = server_node_->CollectiveReceiveAsync(ps::core::NodeRole::SERVER, 0, &recv_str);
    if (!server_node_->CollectiveWait(recv_req_id2, kCollectiveCommTimeout)) {
      MS_LOG(ERROR) << "CollectiveWait " << recv_req_id2 << " failed.";
      return false;
    }
    ret = memcpy_s(output_buff, count * sizeof(T), recv_str->data(), recv_str->size());
    if (ret != 0) {
      MS_LOG(ERROR) << "memcpy_s error, errorno(" << ret << ")";
      return false;
    }
  }
  MS_LOG(DEBUG) << "End broadcast.";
  return true;
}

template <typename T>
bool CollectiveOpsImpl::RingAllGather(const void *sendbuff, void *const recvbuff, size_t send_count) {
  MS_ERROR_IF_NULL_W_RET_VAL(node_, false);
  MS_ERROR_IF_NULL_W_RET_VAL(sendbuff, false);
  MS_ERROR_IF_NULL_W_RET_VAL(recvbuff, false);

  size_t chunk_size = send_count;
  std::vector<size_t> chunk_sizes(rank_size_, chunk_size);

  // Store offsets to get every data chunk's address.
  std::vector<size_t> chunk_offset;
  for (size_t i = 0; i < rank_size_; i++) {
    size_t ofs = std::accumulate(chunk_sizes.begin(), chunk_sizes.begin() + SizeToLong(i), static_cast<size_t>(0),
                                 std::plus<size_t>());
    chunk_offset.push_back(ofs);
  }

  uint32_t send_to_rank = (rank_id_ + 1) % rank_size_;
  uint32_t recv_from_rank = (rank_id_ - 1 + rank_size_) % rank_size_;
  MS_LOG(DEBUG) << "Ring AllGather count:" << send_count << ", rank_size:" << rank_size_ << ", rank_id_:" << rank_id_
                << ", chunk_size:" << chunk_size << ", chunk_sizes:" << chunk_sizes << ", send_to_rank:" << send_to_rank
                << ", recv_from_rank:" << recv_from_rank;

  T *output_buff = reinterpret_cast<T *>(recvbuff);
  size_t src_size = send_count * sizeof(T);
  size_t dst_size = send_count * sizeof(T);
  int ret = memcpy_s(output_buff + chunk_offset[rank_id_], dst_size, sendbuff, src_size);
  if (ret != 0) {
    MS_LOG(ERROR) << "memcpy_s error, errorno(" << ret << ")";
    return false;
  }

  // Ring AllGather.
  MS_LOG(DEBUG) << "Start Ring AllGather.";
  for (size_t i = 0; i < rank_size_ - 1; i++) {
    size_t send_chunk_index = (rank_id_ - i + rank_size_) % rank_size_;
    T *send_chunk = output_buff + chunk_offset[send_chunk_index];
    auto send_req_id =
      node_->CollectiveSendAsync(node_role_, send_to_rank, send_chunk, chunk_sizes[send_chunk_index] * sizeof(T));
    size_t recv_chunk_index = (rank_id_ - i - 1 + rank_size_) % rank_size_;
    T *recv_chunk = output_buff + chunk_offset[recv_chunk_index];
    MS_LOG(DEBUG) << "Ring AllGather send_to_rank:" << send_to_rank << ", recv_from_rank:" << recv_from_rank
                  << ", send count:" << chunk_sizes[send_chunk_index]
                  << ", recv count:" << chunk_sizes[recv_chunk_index] << ", iteration:" << i;

    std::shared_ptr<std::vector<unsigned char>> recv_str;
    auto recv_req_id = node_->CollectiveReceiveAsync(node_role_, recv_from_rank, &recv_str);
    if (!node_->CollectiveWait(recv_req_id, kCollectiveCommTimeout)) {
      MS_LOG(ERROR) << "CollectiveWait " << recv_req_id << " failed.";
      return false;
    }
    ret = memcpy_s(recv_chunk, chunk_sizes[recv_chunk_index] * sizeof(T), recv_str->data(), recv_str->size());
    if (ret != 0) {
      MS_LOG(ERROR) << "memcpy_s error, errorno(" << ret << ")";
      return false;
    }
    if (!node_->Wait(send_req_id, kCollectiveCommTimeout)) {
      MS_LOG(ERROR) << "CollectiveWait " << send_req_id << " failed.";
      return false;
    }
  }
  MS_LOG(DEBUG) << "End Ring AllGather.";
  return true;
}

template <typename T>
bool CollectiveOpsImpl::Broadcast(const void *sendbuff, void *recvbuff, size_t count, uint32_t root,
                                  const CommunicationGroupInfo &group_info) {
  MS_ERROR_IF_NULL_W_RET_VAL(node_, false);
  MS_ERROR_IF_NULL_W_RET_VAL(sendbuff, false);
  MS_ERROR_IF_NULL_W_RET_VAL(recvbuff, false);

  auto group_to_global_ranks = group_info.group_to_global_ranks;
  if (group_to_global_ranks.empty()) {
    MS_LOG(ERROR) << "The group is empty.";
    return false;
  }
  uint32_t group_rank_size = SizeToUint(group_info.group_ranks.size());
  uint32_t global_root_rank = group_to_global_ranks[root];

  // Broadcast data to processes which are not the root.
  MS_LOG(DEBUG) << "Start broadcast from root to other processes.";
  if (rank_id_ == global_root_rank) {
    for (uint32_t i = 1; i < group_rank_size; i++) {
      uint32_t dst_rank = group_to_global_ranks[i];
      MS_LOG(DEBUG) << "Broadcast data to process " << dst_rank;
      auto send_req_id = node_->CollectiveSendAsync(node_role_, dst_rank, sendbuff, count * sizeof(T));
      if (!node_->Wait(send_req_id, kCollectiveCommTimeout)) {
        MS_LOG(ERROR) << "CollectiveWait " << send_req_id << " failed.";
        return false;
      }
    }
  } else {
    MS_LOG(DEBUG) << "Broadcast receive from rank 0.";
    std::shared_ptr<std::vector<unsigned char>> recv_str;
    auto recv_req_id = node_->CollectiveReceiveAsync(node_role_, global_root_rank, &recv_str);
    if (!node_->CollectiveWait(recv_req_id, kCollectiveCommTimeout)) {
      MS_LOG(ERROR) << "CollectiveWait " << recv_req_id << " failed.";
      return false;
    }
    int ret = memcpy_s(recvbuff, count * sizeof(T), recv_str->data(), recv_str->size());
    if (ret != 0) {
      MS_LOG(ERROR) << "memcpy_s error, errorno(" << ret << ")";
      return false;
    }
  }
  MS_LOG(DEBUG) << "End broadcast.";
  return true;
}

template <typename T>
bool CollectiveOpsImpl::AllReduce(const void *sendbuff, void *recvbuff, size_t count) {
  // The collective communication API does not support calling Send and Recv concurrently with multiple threads;
  std::unique_lock<std::mutex> lock(mtx_);
  MS_ERROR_IF_NULL_W_RET_VAL(recvbuff, false);
  MS_ERROR_IF_NULL_W_RET_VAL(sendbuff, false);

  uint32_t rank_size = server_num_;
  if (rank_size == 0) {
    MS_LOG(ERROR) << "Rank size should not be 0.";
    return false;
  }
  if (rank_size_ == 1) {
    MS_LOG(DEBUG) << "Rank size is 1. Do nothing.";
    return true;
  }

  if (count >= rank_size) {
    return RingAllReduce<T>(sendbuff, recvbuff, count);
  } else {
    return ReduceBroadcastAllReduce<T>(sendbuff, recvbuff, count);
  }
}

template <typename T>
bool CollectiveOpsImpl::AllGather(const void *sendbuff, void *const recvbuff, size_t send_count,
                                  const std::shared_ptr<ps::core::AbstractNode> &node) {
  std::unique_lock<std::mutex> lock(mtx_);
  MS_ERROR_IF_NULL_W_RET_VAL(node, false);
  MS_ERROR_IF_NULL_W_RET_VAL(recvbuff, false);
  MS_ERROR_IF_NULL_W_RET_VAL(sendbuff, false);

  // Initialize collective communication parameters.
  node_ = node;
  node_role_ = node_->role();
  rank_id_ = node_->rank_id();
  switch (node_role_) {
    case ps::core::WORKER:
      rank_size_ = IntToUint(node_->worker_num());
      break;
    case ps::core::SERVER:
      rank_size_ = IntToUint(node_->server_num());
      break;
    default:
      MS_LOG(ERROR) << "The node role " << node_role_ << " for collective communication is invalid.";
      return false;
  }
  if (rank_size_ == 0) {
    MS_LOG(ERROR) << "Rank size should not be 0.";
    return false;
  }
  if (rank_size_ == 1) {
    MS_LOG(DEBUG) << "Rank size is 1. Do nothing.";
    return true;
  }

  return RingAllGather<T>(sendbuff, recvbuff, send_count);
}

template <typename T>
bool CollectiveOpsImpl::Broadcast(const void *sendbuff, void *const recvbuff, size_t count, uint32_t root,
                                  const std::shared_ptr<ps::core::AbstractNode> &node,
                                  const CommunicationGroupInfo &group_info) {
  std::unique_lock<std::mutex> lock(mtx_);
  MS_ERROR_IF_NULL_W_RET_VAL(node, false);
  MS_ERROR_IF_NULL_W_RET_VAL(recvbuff, false);
  MS_ERROR_IF_NULL_W_RET_VAL(sendbuff, false);

  // Initialize collective communication parameters.
  node_ = node;
  node_role_ = node_->role();
  rank_id_ = node_->rank_id();
  rank_size_ = group_info.size;
  if (rank_size_ == 0) {
    MS_LOG(ERROR) << "Rank size should not be 0.";
    return false;
  }
  if (rank_size_ == 1) {
    MS_LOG(DEBUG) << "Rank size is 1. Do nothing.";
    return true;
  }

  return Broadcast<T>(sendbuff, recvbuff, count, root, group_info);
}

bool CollectiveOpsImpl::ReInitForScaling() {
  // If CollectiveOpsImpl is not initialized yet but the scaling event is triggered, do not throw exception.
  if (server_node_ == nullptr) {
    return true;
  }

  MS_LOG(INFO) << "Cluster scaling out completed. Reinitialize ring for collective communication.";
  rank_id_ = server_node_->rank_id();
  server_num_ = IntToUint(server_node_->server_num());
  MS_LOG(INFO) << "After scheduler scaling out, this server's rank is " << rank_id_ << ", server number is "
               << server_num_;
  return true;
}

template bool CollectiveOpsImpl::RingAllReduce<float>(const void *sendbuff, void *recvbuff, size_t count);
template bool CollectiveOpsImpl::RingAllReduce<size_t>(const void *sendbuff, void *recvbuff, size_t count);
template bool CollectiveOpsImpl::RingAllReduce<int>(const void *sendbuff, void *recvbuff, size_t count);

template bool CollectiveOpsImpl::ReduceBroadcastAllReduce<float>(const void *sendbuff, void *recvbuff, size_t count);
template bool CollectiveOpsImpl::ReduceBroadcastAllReduce<size_t>(const void *sendbuff, void *recvbuff, size_t count);
template bool CollectiveOpsImpl::ReduceBroadcastAllReduce<int>(const void *sendbuff, void *recvbuff, size_t count);

template bool CollectiveOpsImpl::AllReduce<float>(const void *sendbuff, void *recvbuff, size_t count);
template bool CollectiveOpsImpl::AllReduce<size_t>(const void *sendbuff, void *recvbuff, size_t count);
template bool CollectiveOpsImpl::AllReduce<int>(const void *sendbuff, void *recvbuff, size_t count);

template bool CollectiveOpsImpl::AllGather<float>(const void *sendbuff, void *recvbuff, size_t send_count,
                                                  const std::shared_ptr<ps::core::AbstractNode> &node);
template bool CollectiveOpsImpl::AllGather<uint64_t>(const void *sendbuff, void *recvbuff, size_t send_count,
                                                     const std::shared_ptr<ps::core::AbstractNode> &node);
template bool CollectiveOpsImpl::AllGather<int>(const void *sendbuff, void *recvbuff, size_t send_count,
                                                const std::shared_ptr<ps::core::AbstractNode> &node);
template bool CollectiveOpsImpl::AllGather<char>(const void *sendbuff, void *recvbuff, size_t send_count,
                                                 const std::shared_ptr<ps::core::AbstractNode> &node);

template bool CollectiveOpsImpl::RingAllGather<float>(const void *sendbuff, void *recvbuff, size_t send_count);
template bool CollectiveOpsImpl::RingAllGather<uint64_t>(const void *sendbuff, void *recvbuff, size_t send_count);
template bool CollectiveOpsImpl::RingAllGather<int>(const void *sendbuff, void *recvbuff, size_t send_count);
template bool CollectiveOpsImpl::RingAllGather<char>(const void *sendbuff, void *recvbuff, size_t send_count);

template bool CollectiveOpsImpl::Broadcast<float>(const void *sendbuff, void *recvbuff, size_t count, uint32_t root,
                                                  const std::shared_ptr<ps::core::AbstractNode> &node,
                                                  const CommunicationGroupInfo &group_info);
template bool CollectiveOpsImpl::Broadcast<uint64_t>(const void *sendbuff, void *recvbuff, size_t count, uint32_t root,
                                                     const std::shared_ptr<ps::core::AbstractNode> &node,
                                                     const CommunicationGroupInfo &group_info);
template bool CollectiveOpsImpl::Broadcast<int>(const void *sendbuff, void *recvbuff, size_t count, uint32_t root,
                                                const std::shared_ptr<ps::core::AbstractNode> &node,
                                                const CommunicationGroupInfo &group_info);
template bool CollectiveOpsImpl::Broadcast<char>(const void *sendbuff, void *recvbuff, size_t count, uint32_t root,
                                                 const std::shared_ptr<ps::core::AbstractNode> &node,
                                                 const CommunicationGroupInfo &group_info);

template bool CollectiveOpsImpl::Broadcast<float>(const void *sendbuff, void *recvbuff, size_t count, uint32_t root,
                                                  const CommunicationGroupInfo &group_info);
template bool CollectiveOpsImpl::Broadcast<uint64_t>(const void *sendbuff, void *recvbuff, size_t count, uint32_t root,
                                                     const CommunicationGroupInfo &group_info);
template bool CollectiveOpsImpl::Broadcast<int>(const void *sendbuff, void *recvbuff, size_t count, uint32_t root,
                                                const CommunicationGroupInfo &group_info);
template bool CollectiveOpsImpl::Broadcast<char>(const void *sendbuff, void *recvbuff, size_t count, uint32_t root,
                                                 const CommunicationGroupInfo &group_info);
}  // namespace server
}  // namespace fl
}  // namespace mindspore
