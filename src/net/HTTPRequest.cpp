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
#include "scraps/net/HTTPRequest.h"

#include "scraps/logging.h"
#include "scraps/thread.h"
#include "scraps/utility.h"
#include "scraps/net/curl.h"

#include <gsl.h>

namespace scraps {
namespace net {

HTTPRequest::~HTTPRequest() {
    abort();

    if (_curl) {
        curl_multi_remove_handle(_curlMultiHandle, _curl);
        curl_multi_cleanup(_curlMultiHandle);
        curl_easy_cleanup(_curl);
    }

    if (_curlHeaderList) {
        curl_slist_free_all(_curlHeaderList);
    }
}

void HTTPRequest::initiate(const std::string& url, const void* body, size_t bodyLength, const std::vector<std::string>& headers) {
    if (!CURLIsInitialized()) {
        SCRAPS_LOG_WARNING("cURL may not be initialized properly. You should call scraps::net::InitializeCURL on startup.");
    }

    _curl = curl_easy_init();
    _curlMultiHandle = curl_multi_init();

    curl_easy_setopt(_curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(_curl, CURLOPT_TIMEOUT, 10);
    curl_easy_setopt(_curl, CURLOPT_NOSIGNAL, 1);

    for (auto& header : headers) {
        _curlHeaderList = curl_slist_append(_curlHeaderList, header.c_str());
    }
    if (_curlHeaderList) {
        curl_easy_setopt(_curl, CURLOPT_HTTPHEADER, _curlHeaderList);
    }

    if (_disablePeerVerification) {
        curl_easy_setopt(_curl, CURLOPT_SSL_VERIFYPEER, 0);
    }

    if (body) {
        curl_easy_setopt(_curl, CURLOPT_POST, 1);

        if (bodyLength) {
            _body.assign((const char*)body, bodyLength);
        } else {
            _body.assign((const char*)body);
        }

        curl_easy_setopt(_curl, CURLOPT_POSTFIELDS, _body.data());
        curl_easy_setopt(_curl, CURLOPT_POSTFIELDSIZE, _body.size());
    }

    if (!_caBundlePath.empty()) {
        auto status = curl_easy_setopt(_curl, CURLOPT_CAINFO, _caBundlePath.c_str());
        if (status != CURLE_OK) {
            SCRAPS_LOGF_ERROR("error setting ca bundle (status = %d)", status);
            _error = true;
            return;
        }
    }

    if (!_pinnedKey.empty()) {
        auto status = curl_easy_setopt(_curl, CURLOPT_PINNEDPUBLICKEY, _pinnedKey.c_str());
        if (status != CURLE_OK) {
            SCRAPS_LOGF_ERROR("error pinning public key (status = %d)", status);
            _error = true;
            return;
        }
    }

    curl_easy_setopt(_curl, CURLOPT_WRITEFUNCTION, &CURLWriteCallback);
    curl_easy_setopt(_curl, CURLOPT_WRITEDATA, this);

    curl_easy_setopt(_curl, CURLOPT_HEADERFUNCTION, &CURLHeaderWriteCallback);
    curl_easy_setopt(_curl, CURLOPT_HEADERDATA, this);

    curl_multi_add_handle(_curlMultiHandle, _curl);

    _worker = std::thread([&] {
        SetThreadName("HTTPRequest");
        std::unique_lock<std::mutex> lock{_mutex};

        int stillRunning = 1;
        while (stillRunning) {
            auto status = curl_multi_perform(_curlMultiHandle, &stillRunning);
            if (status != CURLM_OK) {
                _error = true;
                break;
            }

            int n = 0;
            while (auto msg = curl_multi_info_read(_curlMultiHandle, &n)) {
                if (msg->msg == CURLMSG_DONE && msg->data.result) {
                    SCRAPS_LOGF_WARNING("curl result code %d", msg->data.result);
                }
            }

            long timeoutMilliseconds = -1;
            curl_multi_timeout(_curlMultiHandle, &timeoutMilliseconds);
            constexpr long defaultTimeoutMilliseconds = 10;
            if (timeoutMilliseconds < 0) {
                timeoutMilliseconds = defaultTimeoutMilliseconds;
            } else {
                timeoutMilliseconds = std::min(timeoutMilliseconds, defaultTimeoutMilliseconds);
            }

            if (_shouldAbort) { break; }
            _condition.wait_for(lock, std::chrono::milliseconds(timeoutMilliseconds));
            if (_shouldAbort) { break; }
        }

        if (!_error && !_shouldAbort) {
            long responseCode = 0;
            curl_easy_getinfo(_curl, CURLINFO_RESPONSE_CODE, &responseCode);

            if (!responseCode) {
                _error = true;
            } else {
                _responseStatus = responseCode;
                _isComplete = true;
            }
        }
    });
}

void HTTPRequest::wait() {
    if (_worker.joinable()) {
        _worker.join();
    }
}

void HTTPRequest::abort() {
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _shouldAbort = true;
    }
    _condition.notify_all();
    wait();
}

bool HTTPRequest::error() const {
    std::lock_guard<std::mutex> lock{_mutex};
    return _error;
}

bool HTTPRequest::isComplete() const {
    std::lock_guard<std::mutex> lock{_mutex};
    return _isComplete;
}

unsigned int HTTPRequest::responseStatus() const {
    std::lock_guard<std::mutex> lock{_mutex};
    return _responseStatus;
}

std::string HTTPRequest::responseBody() const {
    std::lock_guard<std::mutex> lock{_mutex};
    return _responseBody;
}

std::vector<std::string> HTTPRequest::responseHeaders() const {
    std::lock_guard<std::mutex> lock{_mutex};
    return _responseHeaders;
}

std::vector<std::string> HTTPRequest::responseHeaders(const std::string& name) const {
    std::vector<std::string> ret;
    for (auto& header : responseHeaders()) {
        auto it = std::search(header.begin(), header.end(), name.begin(), name.end(), [](char a, char b) { return std::toupper(a) == std::toupper(b); });
        if (it == header.begin()) {
            std::advance(it, name.size());
            if (it == header.end() || *(it++) != ':') { continue; }
            auto raw = std::string{it, header.end()};
            auto value = gsl::to_string(Trim(gsl::string_span<>{raw}));
            ret.emplace_back(std::move(value));
        }
    }
    return ret;
}

size_t HTTPRequest::CURLWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto request = reinterpret_cast<HTTPRequest*>(userdata);
    request->_responseBody.append(ptr, size * nmemb);
    return size * nmemb;
}

size_t HTTPRequest::CURLHeaderWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto request = reinterpret_cast<HTTPRequest*>(userdata);
    request->_responseHeaders.emplace_back(ptr, size * nmemb);
    return size * nmemb;
}

}} // namespace scraps::net
