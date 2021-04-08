# Copyright 2021 Huawei Technologies Co., Ltd
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
Accelerating.

Provide auto accelerating for network, such as Less BN.
"""
from .less_batch_normalization import *
from .grad_freeze import *

__all__ = ['LessBN', 'FreezeOpt', 'CONTINUOUS_STRATEGY', 'INTERVAL_STRATEGY',
           'split_parameters_groups', 'generate_freeze_index_sequence']
