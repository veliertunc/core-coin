# CoreCoin REST API (v1)

A JSON-over-HTTP API for one CoreCoin node. It reads and writes the same chain
file format as `corecoin-cli --shell`, so you can query blocks, accounts and the
mempool, submit transfers, mine, and sync from a peer from any HTTP client.

> **Prototype — not for real funds.** The API has no authentication. Legacy
> named accounts (such as `founder`) are unsigned, so **any client that can reach
> the API can spend them and mine.** Keep the default `127.0.0.1` binding unless
> you are on a trusted network.

## Contents

- [Running the server](#running-the-server)
- [Conventions](#conventions)
- [Endpoint summary](#endpoint-summary)
- [Node](#node)
- [Blocks](#blocks)
- [Transactions](#transactions)
- [Accounts](#accounts)
- [Peers](#peers)
- [Signing transfers from key-backed wallets](#signing-transfers-from-key-backed-wallets)
- [Errors](#errors)
- [Limits and behaviour](#limits-and-behaviour)

## Running the server

```sh
corecoin-cli --api [CHAIN_FILE] [--bind ADDRESS] [--port PORT] [--cors-origin ORIGIN]
```

| Option | Default | Description |
| --- | --- | --- |
| `CHAIN_FILE` | `corecoin.chain` | Chain file to open, or to create with the genesis block if missing. |
| `--bind` | `127.0.0.1` | IPv4 address to listen on. Use `0.0.0.0` only on trusted networks. |
| `--port` | `18445` | TCP port (the peer protocol uses `18444`). |
| `--cors-origin` | *(off)* | Allow browser calls from one exact origin, or `*` for any origin. |

```sh
python3 scripts/build.py
./build-local/corecoin-cli --api corecoin.chain
# CoreCoin REST API listening on http://127.0.0.1:18445/api/v1 (chain corecoin.chain; Ctrl+C to stop)
```

Each request is logged as `METHOD PATH STATUS`. Press **Ctrl+C** (SIGINT) or send
SIGTERM to stop; mined blocks are already saved.

With Docker Compose, the API is an opt-in profile that stores its chain in
`/data/api.chain` on the shared volume:

```sh
docker compose --profile api up corecoin-api
curl http://127.0.0.1:18445/api/v1/node
```

Don't point the API and an interactive shell at the same chain file at the same
time. Each process keeps its own in-memory state, and the last one to save wins.

Applications can embed the server through the public C++ API:

```cpp
#include <corecoin/module.hpp>

corecoin::RestConfig config;   // bind_address "127.0.0.1", port 18445
config.access_log = [](std::string_view line) { std::cout << line << '\n'; };
corecoin::RestServer server("corecoin.chain", config);
server.run();   // Blocks until server.stop() is called (thread- and signal-safe).
```

## Conventions

- **Base URL:** `http://HOST:PORT/api/v1`
- **Bodies:** `POST` requests need `Content-Type: application/json` and a JSON object body.
  Other content types get `415`.
- **Amounts:** integers in **Gil**, the smallest unit (1 CORE = 100,000,000 Gil).
  Every amount field also has a `*_core` twin: a decimal string with 8 places,
  such as `"12.00000000"`. When you submit, send *either* `amount` (integer Gil) *or*
  `amount_core` (a decimal string with up to 8 places), never both.
- **Hashes and IDs:** 64-character lowercase hexadecimal SHA-256 strings.
- **Addresses:** either legacy names (1–64 characters from `A–Z a–z 0–9 _ -`)
  or key-backed addresses: `cc_` plus 64 lowercase hex characters.
- **Absent values** appear as `null`, for example `sender` on issuance transactions.
- Every response has `Cache-Control: no-store` and `Connection: close`.

### Transaction object

```json
{
  "id": "92ec758e31f3fdd3befdb1d062063666e2ddca93de942e42a4e1336712116205",
  "type": "transfer",
  "sender": "founder",
  "recipient": "bob",
  "amount": 1200000000,
  "amount_core": "12.00000000",
  "nonce": 0,
  "public_key": null,
  "signature": null
}
```

| Field | Description |
| --- | --- |
| `type` | `transfer`, or `issuance` for the genesis allocation and mining rewards (`sender` is `null`). |
| `nonce` | Sender's sequence number. For issuance, this is the block height. |
| `public_key`, `signature` | Hex ECDSA secp256k1 values for key-backed senders; otherwise `null`. |

### Block object

```json
{
  "height": 1,
  "hash": "000d039c81659d876f2b6ed71d0161efc54761e511768a96c95b0fa1f8c9306b",
  "previous_hash": "0cf5b1ebd2df49c50ecd363ba5eafd197da44534028569475005dfb60d8f8744",
  "timestamp": 1794963099,
  "difficulty": 3,
  "nonce": 2637,
  "transaction_count": 2,
  "transactions": [ /* Transaction objects; the last one is the mining reward */ ]
}
```

Block lists return **summaries**, which are the same object without `transactions`.

## Endpoint summary

| Method | Path | Description | Success |
| --- | --- | --- | --- |
| `GET` | `/api/v1` | List all endpoints | 200 |
| `GET` | `/api/v1/health` | Liveness check | 200 |
| `GET` | `/api/v1/node` | Node and network summary | 200 |
| `GET` | `/api/v1/chain/validate` | Replay and validate the whole chain | 200 |
| `GET` | `/api/v1/blocks` | List block summaries | 200 |
| `POST` | `/api/v1/blocks` | Mine pending transactions into a new block | 201 |
| `GET` | `/api/v1/blocks/latest` | Latest block with transactions | 200 |
| `GET` | `/api/v1/blocks/{height}` | Block by height | 200 |
| `GET` | `/api/v1/blocks/hash/{hash}` | Block by hash | 200 |
| `GET` | `/api/v1/transactions/{id}` | Confirmed or pending transaction | 200 |
| `POST` | `/api/v1/transactions` | Submit a transfer to the mempool | 202 |
| `POST` | `/api/v1/transactions/signing-payload` | Build the payload a key-backed sender signs | 200 |
| `GET` | `/api/v1/mempool` | Pending transactions | 200 |
| `GET` | `/api/v1/accounts/{address}` | Balances and next nonce | 200 |
| `GET` | `/api/v1/accounts/{address}/transactions` | Account history | 200 |
| `POST` | `/api/v1/peers/sync` | Fetch a peer's chain and adopt it if longer | 200 |

`OPTIONS` on any path returns `204` (a CORS preflight when `--cors-origin` is set).

---

## Node

### `GET /api/v1`

Machine-readable list of the endpoints in the table above.

```json
{"name": "CoreCoin REST API", "version": "v1",
 "endpoints": [{"method": "GET", "path": "/api/v1/health", "summary": "Liveness check"}, "..."]}
```

### `GET /api/v1/health`

```json
{"status": "ok"}
```

### `GET /api/v1/node`

```json
{
  "network": "corecoin-local",
  "height": 0,
  "tip_hash": "0cf5b1eb…8744",
  "genesis_hash": "0cf5b1eb…8744",
  "difficulty": 3,
  "mining_reward": 1100000000,
  "mining_reward_core": "11.00000000",
  "issued_supply": 1100000000000,
  "issued_supply_core": "11000.00000000",
  "maximum_supply": 1100000000000000,
  "maximum_supply_core": "11000000.00000000",
  "gil_per_core": 100000000,
  "confirmed_transfers": 0,
  "pending_transactions": 0
}
```

`issued_supply` counts the founder allocation plus every mining reward.

### `GET /api/v1/chain/validate`

Replays every block from genesis: hashes, proof of work, links, signatures,
nonces, balances and rewards.

```json
{"valid": true, "height": 1, "error": null}
```

When `valid` is `false`, `error` holds the reason.

## Blocks

### `GET /api/v1/blocks`

| Query | Default | Description |
| --- | --- | --- |
| `order` | `desc` | `desc` lists newest first; `asc` starts from genesis. |
| `offset` | `0` | Number of blocks to skip in the chosen order. |
| `limit` | `20` | Page size, 0–100. |

```sh
curl "http://127.0.0.1:18445/api/v1/blocks?limit=2"
```

```json
{
  "total": 2, "offset": 0, "limit": 2, "order": "desc",
  "blocks": [
    {"height": 1, "hash": "000d039c…306b", "previous_hash": "0cf5b1eb…8744", "timestamp": 1794963099,
     "difficulty": 3, "nonce": 2637, "transaction_count": 2},
    {"height": 0, "hash": "0cf5b1eb…8744", "previous_hash": "0000…0000", "timestamp": 0,
     "difficulty": 0, "nonce": 0, "transaction_count": 1}
  ]
}
```

### `POST /api/v1/blocks` — mine

Mines all pending transactions plus a reward for `miner` into a new block. The
block is saved to the chain file before the response is sent. An empty mempool
still produces a reward-only block.

```sh
curl -X POST http://127.0.0.1:18445/api/v1/blocks \
  -H "Content-Type: application/json" -d '{"miner": "miner"}'
```

| Body field | Type | Required |
| --- | --- | --- |
| `miner` | address string | yes |

**201 Created**: returns the full [block object](#block-object), with a `Location: /api/v1/blocks/{height}` header.
Errors: `400 invalid_miner`.

Mining runs while the node is locked, so other requests wait until it finishes.
That takes milliseconds at the default difficulty.

### `GET /api/v1/blocks/latest`

Returns the tip block with its transactions.

### `GET /api/v1/blocks/{height}`

Returns the block at that height. Errors: `400 invalid_parameter` if the height is
not a non-negative integer, `404 block_not_found`.

### `GET /api/v1/blocks/hash/{hash}`

Returns the block with that hash. Errors: `400 invalid_parameter`, `404 block_not_found`.

## Transactions

### `POST /api/v1/transactions` — submit a transfer

Queues a transfer in the mempool after checking the signature, nonce and
balance (pending-adjusted). It becomes confirmed when a block is mined.

| Body field | Type | Required | Description |
| --- | --- | --- | --- |
| `sender` | address | yes | Paying account. |
| `recipient` | address | yes | Receiving account. Must differ from `sender`. |
| `amount` | integer | one of | Gil, > 0. |
| `amount_core` | string | one of | Decimal CORE with ≤ 8 places, such as `"1.5"`. |
| `nonce` | integer | key-backed: yes | Defaults to the sender's next nonce. Signed transfers must send the nonce they signed. |
| `public_key` | hex string | key-backed only | 33-byte compressed secp256k1 public key (66 hex characters, prefix `02` or `03`). |
| `signature` | hex string | key-backed only | 64-byte ECDSA `r || s` signature with low S; see [signing](#signing-transfers-from-key-backed-wallets). |

Legacy named senders must leave out `public_key` and `signature`.

```sh
curl -i -X POST http://127.0.0.1:18445/api/v1/transactions \
  -H "Content-Type: application/json" \
  -d '{"sender": "founder", "recipient": "bob", "amount_core": "12"}'
```

```http
HTTP/1.1 202 Accepted
Location: /api/v1/transactions/92ec758e31f3fdd3befdb1d062063666e2ddca93de942e42a4e1336712116205
```

```json
{
  "status": "pending",
  "block_height": null,
  "transaction": {"id": "92ec758e…6205", "type": "transfer", "sender": "founder", "recipient": "bob",
                  "amount": 1200000000, "amount_core": "12.00000000", "nonce": 0,
                  "public_key": null, "signature": null}
}
```

Errors:

| Status | Code | When |
| --- | --- | --- |
| 400 | `missing_field`, `invalid_field`, `invalid_amount` | The body is malformed. |
| 422 | `transaction_rejected` | Ledger rules reject it. The message explains why: `Insufficient balance`, `Incorrect transaction nonce`, `Invalid transaction signature`, `Transfer requires two distinct valid accounts`, … |
| 503 | `capacity_exceeded` | The mempool already holds 1,000 transactions. Mine first. |

A rejected submission leaves the mempool unchanged. Pending incoming funds can
be spent by a later transfer in the same block.

### `POST /api/v1/transactions/signing-payload`

Returns the exact string a key-backed sender must sign. It takes the same body as
`POST /transactions` without `signature`, and `public_key` is required. If you
leave out `nonce`, the node fills in the sender's next nonce, including pending
transfers.

```json
{
  "payload": "corecoin-tx-sign-v2|67:cc_4b0e…7d6c3:bob9:2500000001:066:02a1c4…9e7f",
  "encoding": "utf-8",
  "algorithm": "ECDSA-secp256k1-SHA256",
  "nonce": 0,
  "amount": 250000000
}
```

### `GET /api/v1/transactions/{id}`

Looks up a transaction in confirmed blocks first, then in the mempool.

```json
{"status": "confirmed", "block_height": 1, "transaction": { /* Transaction object */ }}
```

For a pending transaction, `status` is `"pending"` and `block_height` is `null`.
Errors: `400 invalid_parameter`, `404 transaction_not_found`.

### `GET /api/v1/mempool`

```json
{"count": 1, "transactions": [ /* Transaction objects in submission order */ ]}
```

The mempool lives in memory only. Restarting the server discards unmined transfers.

## Accounts

### `GET /api/v1/accounts/{address}`

```json
{
  "address": "founder",
  "key_backed": false,
  "balance": 1100000000000,
  "balance_core": "11000.00000000",
  "available_balance": 1100000000000,
  "available_balance_core": "11000.00000000",
  "next_nonce": 0
}
```

| Field | Description |
| --- | --- |
| `balance` | Confirmed balance. |
| `available_balance` | Balance after pending transfers. |
| `next_nonce` | Nonce for the sender's next transfer, counting pending ones. |

An address that has never been used returns zeros. Errors: `400 invalid_address`.

### `GET /api/v1/accounts/{address}/transactions`

```json
{
  "address": "bob",
  "confirmed": [{"status": "confirmed", "block_height": 1, "transaction": { /* … */ }}],
  "pending": []
}
```

Lists every transaction where the address is the sender or the recipient, oldest first.

## Peers

### `POST /api/v1/peers/sync`

Fetches the confirmed chain from a peer running `serve HOST PORT` in the shell
(port 18444 by convention). If the peer's chain is valid, compatible and longer,
the node saves it and switches to it.

```sh
curl -X POST http://127.0.0.1:18445/api/v1/peers/sync \
  -H "Content-Type: application/json" -d '{"host": "127.0.0.1", "port": 18444}'
```

```json
{"adopted": true, "height": 2}
```

`adopted: false` means the local chain is the same length or longer and was kept.

| Status | Code | When |
| --- | --- | --- |
| 400 | `missing_field`, `invalid_field` | `host` is missing, or `port` is outside 1–65535. |
| 409 | `sync_rejected` | The peer is on an incompatible network, or the local node has pending transactions. |
| 502 | `peer_unavailable` | The node couldn't connect, the peer timed out, or the peer sent an invalid response. |

## Signing transfers from key-backed wallets

Key-backed (`cc_…`) accounts need an ECDSA signature over secp256k1 with SHA-256. The server never sees
private keys, so the client signs:

1. `address = "cc_" + sha256_hex("corecoin-address-v2|" + public_key_hex)`
2. `POST /transactions/signing-payload` returns `payload` and `nonce`.
3. Sign the UTF-8 bytes of `payload` with ECDSA (secp256k1, SHA-256). Encode the
   signature as 32-byte big-endian `r` followed by 32-byte `s`, using the low-S
   form (`s <= n/2`; replace `s` with `n - s` otherwise), and hex-encode it.
4. `POST /transactions` with the same fields plus `nonce` and `signature`.

The payload is a length-prefixed encoding (`LEN:VALUE` for each field), so you
can also build it yourself:

```
corecoin-tx-sign-v2|<len>:<sender><len>:<recipient><len>:<amount_gil><len>:<nonce><len>:<public_key_hex>
```

A complete Python client, tested against the server:

```python
import hashlib, json, urllib.request
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.asymmetric.utils import decode_dss_signature
from cryptography.hazmat.primitives.serialization import Encoding, PublicFormat

N = 0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141  # secp256k1 order

API = "http://127.0.0.1:18445/api/v1"

def call(method, path, body=None):
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(API + path, data=data, method=method,
                                 headers={"Content-Type": "application/json"} if data else {})
    with urllib.request.urlopen(req) as response:
        return json.load(response)

def sign(key, message):
    r, s = decode_dss_signature(key.sign(message, ec.ECDSA(hashes.SHA256())))
    s = min(s, N - s)  # low-S form
    return (r.to_bytes(32, "big") + s.to_bytes(32, "big")).hex()

key = ec.generate_private_key(ec.SECP256K1())
public_key = key.public_key().public_bytes(Encoding.X962, PublicFormat.CompressedPoint).hex()
address = "cc_" + hashlib.sha256(f"corecoin-address-v2|{public_key}".encode()).hexdigest()

# Fund the new address from the (unsigned) founder account and confirm it.
call("POST", "/transactions", {"sender": "founder", "recipient": address, "amount_core": "5"})
call("POST", "/blocks", {"miner": "miner"})

# 1. Ask the node for the exact payload, 2. sign it locally, 3. submit.
transfer = {"sender": address, "recipient": "bob", "amount_core": "1.25", "public_key": public_key}
prepared = call("POST", "/transactions/signing-payload", transfer)
signature = sign(key, prepared["payload"].encode("utf-8"))
result = call("POST", "/transactions", {**transfer, "nonce": prepared["nonce"], "signature": signature})
print(result["status"], result["transaction"]["id"])
```

Wallet files created with `wallet-create` in the shell are password-encrypted
PKCS#8 PEM files holding secp256k1 keys. Any ECDSA library that reads encrypted
PKCS#8 can sign
with them.

## Errors

Every error uses the same envelope:

```json
{"error": {"status": 422, "code": "transaction_rejected", "message": "Insufficient balance"}}
```

| Status | Code | Meaning |
| --- | --- | --- |
| 400 | `bad_request` | Malformed HTTP request line, header or percent-encoding. |
| 400 | `invalid_json` | Body is not valid JSON or not a JSON object. |
| 400 | `missing_field` / `invalid_field` | A required field is missing or has the wrong type. |
| 400 | `invalid_amount` | Amount is missing, not positive, both forms sent, or more than 8 decimals. |
| 400 | `invalid_parameter` | Bad path or query parameter. |
| 400 | `invalid_address` | Address fails the format rules. |
| 400 | `invalid_miner` | Miner address rejected. |
| 404 | `not_found` | No such endpoint. |
| 404 | `block_not_found` / `transaction_not_found` | Resource doesn't exist. |
| 405 | `method_not_allowed` | Path exists for another method; the `Allow` header lists them. |
| 408 | `request_timeout` | The full request didn't arrive within 15 seconds. |
| 409 | `sync_rejected` | Peer chain incompatible, or local transactions pending. |
| 413 | `payload_too_large` | Body over 64 KiB. |
| 415 | `unsupported_media_type` | `POST` without `Content-Type: application/json`. |
| 422 | `transaction_rejected` | Transfer breaks ledger rules. |
| 431 | `headers_too_large` | Request line and headers over 16 KiB. |
| 500 | `internal_error` | Unexpected server error. |
| 501 | `not_implemented` | `Transfer-Encoding` (chunked bodies) isn't supported. |
| 502 | `peer_unavailable` | Peer sync failed at the network level. |
| 503 | `capacity_exceeded` | Mempool full (1,000 pending). |
| 505 | `http_version_not_supported` | Only HTTP/1.0 and HTTP/1.1 are accepted. |

## Limits and behaviour

- **One request at a time.** Each connection gets one response and is then
  closed (no keep-alive). Node state is guarded by a mutex, so the server is
  safe to embed in a multithreaded host.
- **Sizes:** request head ≤ 16 KiB, body ≤ 64 KiB (`Content-Length` required; chunked bodies aren't supported).
- **Persistence:** mined and synced blocks are written atomically before they
  become visible. Pending transactions are in memory only.
- **Lookups** by transaction ID and account history scan the whole chain. That's
  fine for this prototype, but there is no index.
- **CORS** is off by default. Requiring `application/json` on `POST` forces
  browsers to send a preflight request. With CORS off, the browser blocks
  cross-site writes. Enable `--cors-origin` only for a front-end you trust: every
  allowed origin can spend legacy accounts.
- **Security:** no TLS, no authentication, no rate limiting. Put a reverse proxy
  in front (TLS, auth, limits) before exposing it beyond localhost.
