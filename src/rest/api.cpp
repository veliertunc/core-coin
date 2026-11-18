#include "rest/api.hpp"
#include "rest/presenter.hpp"
#include "ledger/address.hpp"
#include "corecoin/signing_key.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <limits>
#include <stdexcept>

namespace corecoin::__rest {
namespace {
constexpr std::size_t default_page_size = 20;
constexpr std::size_t max_page_size = 100;

struct EndpointInfo { std::string_view method, path, summary; };
constexpr std::array endpoints{
    EndpointInfo{"GET", "/api/v1", "List endpoints"},
    EndpointInfo{"GET", "/api/v1/health", "Liveness check"},
    EndpointInfo{"GET", "/api/v1/node", "Node and network summary"},
    EndpointInfo{"GET", "/api/v1/chain/validate", "Replay and validate the full chain"},
    EndpointInfo{"GET", "/api/v1/blocks", "List block summaries (offset, limit, order)"},
    EndpointInfo{"POST", "/api/v1/blocks", "Mine pending transactions into a new block"},
    EndpointInfo{"GET", "/api/v1/blocks/latest", "Latest block with transactions"},
    EndpointInfo{"GET", "/api/v1/blocks/hash/{hash}", "Block by hash"},
    EndpointInfo{"GET", "/api/v1/blocks/{height}", "Block by height"},
    EndpointInfo{"GET", "/api/v1/transactions/{id}", "Confirmed or pending transaction by id"},
    EndpointInfo{"POST", "/api/v1/transactions", "Submit a transfer to the mempool"},
    EndpointInfo{"POST", "/api/v1/transactions/signing-payload", "Build the payload a key-backed sender must sign"},
    EndpointInfo{"GET", "/api/v1/mempool", "Pending transactions"},
    EndpointInfo{"GET", "/api/v1/accounts/{address}", "Balances and next nonce"},
    EndpointInfo{"GET", "/api/v1/accounts/{address}/transactions", "Account transaction history"},
    EndpointInfo{"POST", "/api/v1/peers/sync", "Fetch a peer chain and adopt it if longer"},
};

[[noreturn]] void invalid(const std::string& code, const std::string& message) { throw HttpError(400, code, message); }

std::uint64_t parse_uint(std::string_view text, const std::string& name) {
    std::uint64_t value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (text.empty() || error != std::errc{} || end != text.data() + text.size())
        invalid("invalid_parameter", name + " must be a non-negative integer");
    return value;
}
std::size_t query_size(const HttpRequest& request, const std::string& name, std::size_t fallback, std::size_t maximum) {
    const auto text = request.query_value(name);
    if (!text) return fallback;
    const auto value = parse_uint(*text, name);
    if (value > maximum) invalid("invalid_parameter", name + " must be at most " + std::to_string(maximum));
    return static_cast<std::size_t>(value);
}
bool is_hash(std::string_view value) {
    return value.size() == 64 && std::ranges::all_of(value, [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}
std::string address_param(const HttpRequest& request) {
    const auto& address = request.params.at("address");
    if (!__detail::valid_account(address)) invalid("invalid_address", "Invalid account address");
    return address;
}

Json parse_body(const HttpRequest& request) {
    const auto* type = request.header("content-type");
    std::string media = type ? type->substr(0, type->find(';')) : std::string{};
    std::ranges::transform(media, media.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    while (!media.empty() && media.back() == ' ') media.pop_back();
    if (media != "application/json")
        throw HttpError(415, "unsupported_media_type", "Content-Type must be application/json");
    Json body;
    try { body = Json::parse(request.body); }
    catch (const JsonError& error) { invalid("invalid_json", error.what()); }
    if (!body.is_object()) invalid("invalid_json", "Request body must be a JSON object");
    return body;
}
const Json* field(const Json& body, const std::string& name, bool required) {
    const auto* value = body.find(name);
    if (value && value->is_null()) value = nullptr;
    if (!value && required) invalid("missing_field", "Missing required field: " + name);
    return value;
}
std::string string_field(const Json& body, const std::string& name, bool required = true) {
    const auto* value = field(body, name, required);
    if (!value) return {};
    if (!value->is_string()) invalid("invalid_field", name + " must be a string");
    return value->as_string();
}
std::optional<std::uint64_t> uint_field(const Json& body, const std::string& name, bool required = false) {
    const auto* value = field(body, name, required);
    if (!value) return std::nullopt;
    if (!value->is_integer() || value->as_integer() < 0) invalid("invalid_field", name + " must be a non-negative integer");
    return static_cast<std::uint64_t>(value->as_integer());
}
Amount amount_field(const Json& body) {
    const auto* gil = field(body, "amount", false);
    const auto* core = field(body, "amount_core", false);
    if ((gil != nullptr) == (core != nullptr))
        invalid("invalid_amount", "Provide exactly one of amount (integer Gil) or amount_core (decimal string)");
    if (gil) {
        if (!gil->is_integer() || gil->as_integer() <= 0) invalid("invalid_amount", "amount must be a positive integer number of Gil");
        return gil->as_integer();
    }
    if (!core->is_string()) invalid("invalid_amount", "amount_core must be a decimal string such as \"1.5\"");
    try {
        const auto amount = parse_core_amount(core->as_string());
        if (amount <= 0) invalid("invalid_amount", "amount_core must be positive");
        return amount;
    } catch (const HttpError&) { throw; }
    catch (const std::exception& error) { invalid("invalid_amount", error.what()); }
}
TransferRequest transfer_request(const Json& body) {
    TransferRequest request;
    request.sender = string_field(body, "sender");
    request.recipient = string_field(body, "recipient");
    request.amount = amount_field(body);
    request.nonce = uint_field(body, "nonce");
    request.public_key = string_field(body, "public_key", false);
    request.signature = string_field(body, "signature", false);
    return request;
}

// Maps library exceptions from a state-changing call to API errors.
template <class F>
decltype(auto) domain_call(F&& call, int status, const std::string& code) {
    try {
        return std::forward<F>(call)();
    } catch (const HttpError&) {
        throw;
    } catch (const std::length_error& error) {
        throw HttpError(503, "capacity_exceeded", error.what());
    } catch (const std::invalid_argument& error) {
        throw HttpError(status, code, error.what());
    } catch (const std::overflow_error& error) {
        throw HttpError(status, code, error.what());
    }
}
}

HttpResponse json_response(int status, const Json& body) {
    HttpResponse response;
    response.status = status;
    response.add_header("Content-Type", "application/json; charset=utf-8");
    response.body = body.dump();
    response.body += '\n';
    return response;
}

HttpResponse error_response(int status, const std::string& code, const std::string& message) {
    auto error = Json::object();
    error.set("status", status).set("code", code).set("message", message);
    auto body = Json::object();
    body.set("error", std::move(error));
    return json_response(status, body);
}

Api::Api(NodeService& node, ApiOptions options) : node_(node), options_(std::move(options)) { register_routes(); }

void Api::register_routes() {
    const auto route = [this](std::string method, std::string_view suffix, Router::Handler handler) {
        router_.add(std::move(method), std::string(api_prefix) + std::string(suffix), std::move(handler));
    };

    route("GET", "", [](const HttpRequest&) {
        auto list = Json::array();
        for (const auto& endpoint : endpoints) {
            auto item = Json::object();
            item.set("method", endpoint.method).set("path", endpoint.path).set("summary", endpoint.summary);
            list.push(std::move(item));
        }
        auto body = Json::object();
        body.set("name", "CoreCoin REST API").set("version", "v1").set("endpoints", std::move(list));
        return json_response(200, body);
    });
    route("GET", "/health", [](const HttpRequest&) {
        auto body = Json::object();
        body.set("status", "ok");
        return json_response(200, body);
    });
    route("GET", "/node", [this](const HttpRequest&) {
        return json_response(200, node_.read([](const Blockchain& chain) { return Presenter::node(chain); }));
    });
    route("GET", "/chain/validate", [this](const HttpRequest&) {
        return node_.read([](const Blockchain& chain) {
            std::string error;
            const bool valid = chain.validate(&error);
            auto body = Json::object();
            body.set("valid", valid)
                .set("height", chain.blocks().back().height())
                .set("error", valid ? Json(nullptr) : Json(error));
            return json_response(200, body);
        });
    });

    route("GET", "/blocks", [this](const HttpRequest& request) {
        const auto offset = query_size(request, "offset", 0, std::numeric_limits<std::size_t>::max());
        const auto limit = query_size(request, "limit", default_page_size, max_page_size);
        const auto order = request.query_value("order").value_or("desc");
        if (order != "asc" && order != "desc") invalid("invalid_parameter", "order must be asc or desc");
        return node_.read([&](const Blockchain& chain) {
            const auto& blocks = chain.blocks();
            auto list = Json::array();
            for (std::size_t i = offset; i < blocks.size() && i - offset < limit; ++i)
                list.push(Presenter::block_summary(order == "asc" ? blocks[i] : blocks[blocks.size() - 1 - i]));
            auto body = Json::object();
            body.set("total", blocks.size()).set("offset", offset).set("limit", limit)
                .set("order", order).set("blocks", std::move(list));
            return json_response(200, body);
        });
    });
    route("POST", "/blocks", [this](const HttpRequest& request) {
        const auto body = parse_body(request);
        const auto miner = string_field(body, "miner");
        const auto block = domain_call([&] { return node_.mine(miner); }, 400, "invalid_miner");
        auto response = json_response(201, Presenter::block(block));
        response.add_header("Location", std::string(api_prefix) + "/blocks/" + std::to_string(block.height()));
        return response;
    });
    route("GET", "/blocks/latest", [this](const HttpRequest&) {
        return json_response(200, node_.read([](const Blockchain& chain) { return Presenter::block(chain.blocks().back()); }));
    });
    route("GET", "/blocks/hash/{hash}", [this](const HttpRequest& request) {
        const auto& hash = request.params.at("hash");
        if (!is_hash(hash)) invalid("invalid_parameter", "hash must be 64 lowercase hexadecimal characters");
        return node_.read([&](const Blockchain& chain) {
            for (const auto& block : chain.blocks())
                if (block.hash() == hash) return json_response(200, Presenter::block(block));
            throw HttpError(404, "block_not_found", "No block with hash " + hash);
        });
    });
    route("GET", "/blocks/{height}", [this](const HttpRequest& request) {
        const auto height = parse_uint(request.params.at("height"), "height");
        return node_.read([&](const Blockchain& chain) {
            if (height >= chain.blocks().size())
                throw HttpError(404, "block_not_found", "No block at height " + std::to_string(height));
            return json_response(200, Presenter::block(chain.blocks()[static_cast<std::size_t>(height)]));
        });
    });

    route("GET", "/transactions/{id}", [this](const HttpRequest& request) {
        const auto& id = request.params.at("id");
        if (!is_hash(id)) invalid("invalid_parameter", "id must be 64 lowercase hexadecimal characters");
        return node_.read([&](const Blockchain& chain) {
            for (const auto& block : chain.blocks())
                for (const auto& tx : block.transactions())
                    if (tx.id() == id) return json_response(200, Presenter::located_transaction(tx, block.height()));
            for (const auto& tx : chain.pending())
                if (tx.id() == id) return json_response(200, Presenter::located_transaction(tx, std::nullopt));
            throw HttpError(404, "transaction_not_found", "No transaction with id " + id);
        });
    });
    route("POST", "/transactions", [this](const HttpRequest& request) {
        const auto transfer = transfer_request(parse_body(request));
        const auto tx = domain_call([&] { return node_.submit(transfer); }, 422, "transaction_rejected");
        auto response = json_response(202, Presenter::located_transaction(tx, std::nullopt));
        response.add_header("Location", std::string(api_prefix) + "/transactions/" + tx.id());
        return response;
    });
    route("POST", "/transactions/signing-payload", [this](const HttpRequest& request) {
        auto transfer = transfer_request(parse_body(request));
        if (transfer.public_key.empty()) invalid("missing_field", "Missing required field: public_key");
        transfer.signature.clear();
        const auto tx = node_.build_transfer(transfer);
        auto body = Json::object();
        body.set("payload", tx.signing_payload())
            .set("encoding", "utf-8")
            .set("algorithm", std::string(SigningKey::algorithm))
            .set("nonce", tx.nonce())
            .set("amount", tx.amount());
        return json_response(200, body);
    });
    route("GET", "/mempool", [this](const HttpRequest&) {
        return node_.read([](const Blockchain& chain) {
            auto list = Json::array();
            for (const auto& tx : chain.pending()) list.push(Presenter::transaction(tx));
            auto body = Json::object();
            body.set("count", chain.pending().size()).set("transactions", std::move(list));
            return json_response(200, body);
        });
    });

    route("GET", "/accounts/{address}", [this](const HttpRequest& request) {
        const auto address = address_param(request);
        return json_response(200, node_.read([&](const Blockchain& chain) { return Presenter::account(chain, address); }));
    });
    route("GET", "/accounts/{address}/transactions", [this](const HttpRequest& request) {
        const auto address = address_param(request);
        return node_.read([&](const Blockchain& chain) {
            auto confirmed = Json::array(), pending = Json::array();
            for (const auto& block : chain.blocks())
                for (const auto& tx : block.transactions())
                    if (tx.sender() == address || tx.recipient() == address)
                        confirmed.push(Presenter::located_transaction(tx, block.height()));
            for (const auto& tx : chain.pending())
                if (tx.sender() == address || tx.recipient() == address)
                    pending.push(Presenter::located_transaction(tx, std::nullopt));
            auto body = Json::object();
            body.set("address", address).set("confirmed", std::move(confirmed)).set("pending", std::move(pending));
            return json_response(200, body);
        });
    });

    route("POST", "/peers/sync", [this](const HttpRequest& request) {
        const auto body = parse_body(request);
        const auto host = string_field(body, "host");
        const auto port = *uint_field(body, "port", true);
        if (host.empty() || port == 0 || port > 65535) invalid("invalid_field", "host is required and port must be 1..65535");
        SyncResult result;
        try {
            result = domain_call([&] { return node_.sync(host, static_cast<std::uint16_t>(port)); }, 409, "sync_rejected");
        } catch (const HttpError&) {
            throw;
        } catch (const std::exception& error) {
            throw HttpError(502, "peer_unavailable", error.what());
        }
        auto response = Json::object();
        response.set("adopted", result.adopted).set("height", result.height);
        return json_response(200, response);
    });
}

void Api::apply_cors(const HttpRequest& request, HttpResponse& response) const {
    if (options_.cors_origin.empty()) return;
    const auto* origin = request.header("origin");
    if (options_.cors_origin == "*") response.add_header("Access-Control-Allow-Origin", "*");
    else if (origin && *origin == options_.cors_origin) {
        response.add_header("Access-Control-Allow-Origin", *origin);
        response.add_header("Vary", "Origin");
    }
}

HttpResponse Api::handle(const HttpRequest& request) const {
    HttpResponse response;
    try {
        if (request.method == "OPTIONS") {
            response.status = 204;
            response.add_header("Allow", "GET, POST, OPTIONS");
            if (!options_.cors_origin.empty()) {
                response.add_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
                response.add_header("Access-Control-Allow-Headers", "Content-Type");
                response.add_header("Access-Control-Max-Age", "600");
            }
        } else response = router_.dispatch(request);
    } catch (const HttpError& error) {
        response = error_response(error.status(), error.code(), error.what());
        if (error.status() == 405) response.add_header("Allow", error.what() + std::string_view("Allowed methods: ").size());
    } catch (const std::exception& error) {
        response = error_response(500, "internal_error", error.what());
    }
    response.add_header("Cache-Control", "no-store");
    apply_cors(request, response);
    return response;
}
}
