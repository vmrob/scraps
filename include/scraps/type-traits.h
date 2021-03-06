/**
* Copyright 2016 BitTorrent Inc.
* 
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* 
*    http://www.apache.org/licenses/LICENSE-2.0
* 
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
#pragma once

#include "scraps/config.h"

namespace scraps {

template<typename T>
struct RemoveCVR : std::remove_cv<std::remove_reference_t<T>>{};

template <typename T>
using RemoveCVRType = typename RemoveCVR<T>::type;

template <typename T, typename U>
struct EnableIfSame : std::enable_if<std::is_same<T, U>::value> {};

template <typename T, typename U>
using EnableIfSameType = typename EnableIfSame<T, U>::type;

} // namespace scraps
