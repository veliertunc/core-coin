#include "module.hpp"
#include "rest/api.hpp"
#include "rest/http.hpp"
#include "rest/json.hpp"
#include "rest/node_service.hpp"
#include "rest/router.hpp"
#include <filesystem>
#include <random>
#include <string>

namespace corecoin::__tests {
using namespace corecoin::__rest;

namespace {
class TempChain {
public:
    TempChain() : path_(std::filesystem::temp_directory_path() /
                        ("corecoin-rest-" + std::to_string(std::random_device{}()) + ".chain")) {}
    ~TempChain() { std::filesystem::remove(path_); }
    [[nodiscard]] const std::filesystem::path& path() const { return path_; }
private:
    std::filesystem::path path_;
};

HttpRequest request(std::string method, std::string path, std::string body = {}) {
    HttpRequest result;
    result.method = std::move(method);
    const auto question = path.find('?');
    result.path = path.substr(0, question);
    if (question != std::string::npos) {
        const auto parsed = parse_request_head("GET " + path + " HTTP/1.1");
        result.query = parsed.query;
    }
    if (!body.empty()) {
        result.headers["content-type"] = "application/json";
        result.body = std::move(body);
    }
    return result;
}
Json body_of(const HttpResponse& response) { return Json::parse(response.body); }
}

TEST(RestJsonTests, RoundTripsNestedValues) {
    const auto json = Json::parse(R"({"a":[1,-2,true,null,"x\ny\u00e9\ud83d\ude00"],"b":{"c":1.5}})");
    EXPECT_EQ(json.find("a")->as_array()[1].as_integer(), -2);
    EXPECT_EQ(json.find("a")->as_array()[4].as_string(), "x\ny\xC3\xA9\xF0\x9F\x98\x80");
    EXPECT_EQ(Json::parse(json.dump()).dump(), json.dump());
    EXPECT_EQ(Json("q\"\\\x01").dump(), R"("q\"\\\u0001")");
}
TEST(RestJsonTests, RejectsMalformedDocuments) {
    for (const auto* text : {"", "{", "[1,]", "{\"a\":1,\"a\":2}", "01", "1 2", "\"\\ud800\"", "nul", "{\"a\" 1}"})
        EXPECT_THROW((void)Json::parse(text), JsonError) << text;
    EXPECT_THROW((void)Json::parse(std::string(40, '[') + std::string(40, ']')), JsonError);
}

TEST(RestHttpTests, ParsesRequestHead) {
    const auto parsed = parse_request_head("GET /api/v1/blocks?limit=5&order=asc&q=a+b%21 HTTP/1.1\r\n"
                                           "Host: localhost\r\nContent-Type: application/json \r\n");
    EXPECT_EQ(parsed.method, "GET");
    EXPECT_EQ(parsed.path, "/api/v1/blocks");
    EXPECT_EQ(parsed.query.at("limit"), "5");
    EXPECT_EQ(parsed.query.at("q"), "a b!");
    EXPECT_EQ(*parsed.header("CONTENT-TYPE"), "application/json");
}
TEST(RestHttpTests, RejectsInvalidHeads) {
    EXPECT_THROW((void)parse_request_head("GET"), HttpError);
    EXPECT_THROW((void)parse_request_head("GET relative HTTP/1.1"), HttpError);
    EXPECT_THROW((void)parse_request_head("GET / HTTP/1.1\r\nno-colon"), HttpError);
    EXPECT_THROW((void)parse_request_head("GET / HTTP/1.1\r\nContent-Length: 1\r\nContent-Length: 2"), HttpError);
    auto chunked = parse_request_head("POST / HTTP/1.1\r\nTransfer-Encoding: chunked");
    EXPECT_THROW((void)content_length(chunked), HttpError);
    auto huge = parse_request_head("POST / HTTP/1.1\r\nContent-Length: 999999999");
    EXPECT_THROW((void)content_length(huge), HttpError);
}

TEST(RestRouterTests, MatchesLiteralsBeforeParametersAndReports405) {
    Router router;
    router.add("GET", "/items/latest", [](const HttpRequest&) { HttpResponse r; r.body = "latest"; return r; });
    router.add("GET", "/items/{id}", [](const HttpRequest& req) { HttpResponse r; r.body = req.params.at("id"); return r; });
    EXPECT_EQ(router.dispatch(request("GET", "/items/latest")).body, "latest");
    EXPECT_EQ(router.dispatch(request("GET", "/items/a%20b")).body, "a b");
    try {
        (void)router.dispatch(request("POST", "/items/1"));
        FAIL() << "Expected 405";
    } catch (const HttpError& error) { EXPECT_EQ(error.status(), 405); }
    try {
        (void)router.dispatch(request("GET", "/missing"));
        FAIL() << "Expected 404";
    } catch (const HttpError& error) { EXPECT_EQ(error.status(), 404); }
}

TEST(RestApiTests, SubmitsMinesAndQueries) {
    TempChain file;
    NodeService node(file.path());
    const Api api(node);
    auto submitted = api.handle(request("POST", "/api/v1/transactions",
                                        R"({"sender":"founder","recipient":"bob","amount_core":"2"})"));
    ASSERT_EQ(submitted.status, 202) << submitted.body;
    const auto id = body_of(submitted).find("transaction")->find("id")->as_string();

    auto mined = api.handle(request("POST", "/api/v1/blocks", R"({"miner":"miner"})"));
    ASSERT_EQ(mined.status, 201) << mined.body;
    EXPECT_EQ(body_of(mined).find("height")->as_integer(), 1);

    auto account = body_of(api.handle(request("GET", "/api/v1/accounts/bob")));
    EXPECT_EQ(account.find("balance")->as_integer(), 2 * gil_per_coin);
    auto located = body_of(api.handle(request("GET", "/api/v1/transactions/" + id)));
    EXPECT_EQ(located.find("status")->as_string(), "confirmed");

    // The mined block was persisted before becoming visible.
    EXPECT_EQ(ChainStore(file.path()).load().blocks().size(), 2u);
    auto page = body_of(api.handle(request("GET", "/api/v1/blocks?order=asc&limit=1&offset=1")));
    EXPECT_EQ(page.find("blocks")->as_array().at(0).find("height")->as_integer(), 1);
}
TEST(RestApiTests, RejectedTransferLeavesStateUnchanged) {
    TempChain file;
    NodeService node(file.path());
    const Api api(node);
    auto rejected = api.handle(request("POST", "/api/v1/transactions",
                                       R"({"sender":"nobody","recipient":"bob","amount":1})"));
    EXPECT_EQ(rejected.status, 422);
    EXPECT_EQ(body_of(rejected).find("error")->find("code")->as_string(), "transaction_rejected");
    EXPECT_EQ(body_of(api.handle(request("GET", "/api/v1/mempool"))).find("count")->as_integer(), 0);
}
TEST(RestApiTests, ValidatesRequests) {
    TempChain file;
    NodeService node(file.path());
    const Api api(node);
    EXPECT_EQ(api.handle(request("GET", "/api/v1/blocks/7")).status, 404);
    EXPECT_EQ(api.handle(request("GET", "/api/v1/blocks?limit=101")).status, 400);
    EXPECT_EQ(api.handle(request("GET", "/api/v1/accounts/has%20space")).status, 400);
    EXPECT_EQ(api.handle(request("POST", "/api/v1/blocks", "[]")).status, 400);
    EXPECT_EQ(api.handle(request("POST", "/api/v1/transactions",
                                 R"({"sender":"founder","recipient":"bob","amount":-1})")).status, 400);
    auto plain = request("POST", "/api/v1/blocks", R"({"miner":"m"})");
    plain.headers["content-type"] = "text/plain";
    EXPECT_EQ(api.handle(plain).status, 415);
}
TEST(RestApiTests, AppliesConfiguredCorsOrigin) {
    TempChain file;
    NodeService node(file.path());
    const Api api(node, {"https://wallet.example"});
    auto preflight = request("OPTIONS", "/api/v1/transactions");
    preflight.headers["origin"] = "https://wallet.example";
    const auto response = api.handle(preflight);
    EXPECT_EQ(response.status, 204);
    bool allowed = false;
    for (const auto& [name, value] : response.headers)
        allowed |= name == "Access-Control-Allow-Origin" && value == "https://wallet.example";
    EXPECT_TRUE(allowed);
}
}
