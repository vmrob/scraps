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

#include <unordered_map>

namespace scraps {

class URL {
public:
    explicit URL(const std::string& url) : _url{url} {}

    const std::string& str() const { return _url; }

    std::string protocol() const;

    std::string host() const;

    /**
    * The resource string. E.g. for "google.com/thing/a?q", "/thing/a?q" is returned.
    */
    std::string resource() const;

    /**
    * The path component. E.g. for "google.com/thing/a?q", "/thing/a" is returned.
    */
    std::string path() const;

    /**
    * The query string. E.g. for "google.com/thing/a?q", "q" is returned.
    */
    std::string query() const;

    /**
    * The port. This may be inferred from the protocol or 0 if unknown.
    */
    uint16_t port() const;

    static std::unordered_map<std::string, std::string> ParseQuery(const std::string& query);

private:
    std::string _url;
};

} // namespace scraps
