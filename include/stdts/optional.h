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

#if __has_include(<optional>)

#include <optional>
namespace stdts {
    using std::optional;
    using std::nullopt_t;
    using std::in_place_t;
    using std::bad_optional_access;
    using std::nullopt;
    using std::in_place;
    using std::make_optional;
} // namespace stdts

#elif __has_include(<experimental/optional>)

#include <experimental/optional>
namespace stdts {
    using std::experimental::optional;
    using std::experimental::nullopt_t;
    using std::experimental::in_place_t;
    using std::experimental::bad_optional_access;
    using std::experimental::nullopt;
    using std::experimental::in_place;
    using std::experimental::make_optional;
} // namespace stdts

#else
    #error "an implementation of optional is required!"
#endif
