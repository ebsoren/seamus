#pragma once

#include "../lib/string.h"
#include "../lib/vector.h"
#include "../ranker/Ranker.h"


// TODO: implement RPC infra for query handler
inline bool send_word_request(const string& host, uint16_t port, const string& word) {
    return true;
}

inline vector<RankedPage> recv_word_response(int fd) {
    return vector<RankedPage>{};
}

inline vector<RankedPage> recv_word_request(int fd) {
    return {};
}

inline bool send_word_response(const string& host, uint16_t port, const vector<RankedPage>& results) {
    return true;
}
