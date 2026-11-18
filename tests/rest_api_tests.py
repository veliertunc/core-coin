"""Start corecoin-cli --api and exercise every REST endpoint over real HTTP."""
import json
import signal
import socket
import subprocess
import sys
import tempfile
import threading
import time
import urllib.error
import urllib.request
from pathlib import Path

exe = sys.argv[1]


def unused_port():
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        return listener.getsockname()[1]


class Api:
    def __init__(self, chain, port, *extra):
        self.base = f"http://127.0.0.1:{port}/api/v1"
        self.process = subprocess.Popen([exe, "--api", str(chain), "--port", str(port), *extra],
                                        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, bufsize=1)
        self.output = []
        ready = threading.Event()

        def read():
            for line in self.process.stdout:
                self.output.append(line)
                if "listening on" in line:
                    ready.set()
        threading.Thread(target=read, daemon=True).start()
        if not ready.wait(15):
            self.process.kill()
            raise RuntimeError("API did not start: " + "".join(self.output) + self.process.stderr.read())

    def call(self, method, path, body=None, content_type="application/json", raw=None, headers=None):
        data = raw if raw is not None else (json.dumps(body).encode() if body is not None else None)
        request = urllib.request.Request(self.base + path, data=data, method=method, headers=headers or {})
        if data is not None and content_type:
            request.add_header("Content-Type", content_type)
        try:
            with urllib.request.urlopen(request, timeout=30) as response:
                text = response.read().decode()
                return response.status, json.loads(text) if text else None, response.headers
        except urllib.error.HTTPError as error:
            text = error.read().decode()
            return error.code, json.loads(text) if text else None, error.headers

    def stop(self):
        self.process.send_signal(signal.SIGINT)
        try:
            self.process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            self.process.kill()
            raise RuntimeError("API did not stop on SIGINT")
        if self.process.returncode != 0:
            raise RuntimeError("API exited with an error: " + self.process.stderr.read())


def expect(condition, message, payload=None):
    if not condition:
        raise RuntimeError(f"{message}: {json.dumps(payload, indent=2) if payload is not None else ''}")


def expect_error(result, status, code):
    actual_status, body, _ = result
    expect(actual_status == status and body["error"]["code"] == code,
           f"Expected {status} {code}, got {actual_status}", body)


with tempfile.TemporaryDirectory() as directory:
    chain = Path(directory) / "api.chain"
    port = unused_port()
    api = Api(chain, port)
    try:
        status, index, _ = api.call("GET", "")
        expect(status == 200 and len(index["endpoints"]) >= 15, "Endpoint index", index)
        status, health, _ = api.call("GET", "/health")
        expect(status == 200 and health == {"status": "ok"}, "Health", health)

        status, node, _ = api.call("GET", "/node")
        expect(status == 200 and node["height"] == 0 and node["issued_supply_core"] == "11000.00000000",
               "Fresh node summary", node)

        status, account, _ = api.call("GET", "/accounts/founder")
        expect(account["balance"] == 1_100_000_000_000 and account["next_nonce"] == 0, "Founder account", account)

        # Submit with integer Gil and with a decimal CORE string; nonces are assigned in order.
        status, first, headers = api.call("POST", "/transactions",
                                          {"sender": "founder", "recipient": "bob", "amount": 1_200_000_000})
        expect(status == 202 and first["status"] == "pending" and first["transaction"]["nonce"] == 0, "Submit", first)
        expect(headers["Location"].endswith(first["transaction"]["id"]), "Location header", dict(headers))
        status, second, _ = api.call("POST", "/transactions",
                                     {"sender": "founder", "recipient": "carol", "amount_core": "0.5"})
        expect(status == 202 and second["transaction"]["amount"] == 50_000_000 and
               second["transaction"]["nonce"] == 1, "Submit amount_core", second)

        status, mempool, _ = api.call("GET", "/mempool")
        expect(mempool["count"] == 2, "Mempool", mempool)
        status, bob, _ = api.call("GET", "/accounts/bob")
        expect(bob["balance"] == 0 and bob["available_balance_core"] == "12.00000000", "Pending balance", bob)

        status, pending_tx, _ = api.call("GET", "/transactions/" + first["transaction"]["id"])
        expect(status == 200 and pending_tx["status"] == "pending" and pending_tx["block_height"] is None,
               "Pending lookup", pending_tx)

        # Domain rejections leave state unchanged.
        expect_error(api.call("POST", "/transactions", {"sender": "dave", "recipient": "founder", "amount": 5}),
                     422, "transaction_rejected")
        expect_error(api.call("POST", "/transactions", {"sender": "founder", "recipient": "bob", "amount": 1,
                                                        "nonce": 0}), 422, "transaction_rejected")

        status, block, headers = api.call("POST", "/blocks", {"miner": "miner"})
        expect(status == 201 and block["height"] == 1 and len(block["transactions"]) == 3 and
               block["transactions"][-1]["type"] == "issuance" and block["hash"].startswith("000"), "Mine", block)
        expect(headers["Location"].endswith("/blocks/1"), "Mine Location", dict(headers))

        status, confirmed, _ = api.call("GET", "/transactions/" + first["transaction"]["id"])
        expect(confirmed["status"] == "confirmed" and confirmed["block_height"] == 1, "Confirmed lookup", confirmed)
        status, bob, _ = api.call("GET", "/accounts/bob")
        expect(bob["balance_core"] == "12.00000000", "Confirmed balance", bob)

        status, history, _ = api.call("GET", "/accounts/founder/transactions")
        expect(len(history["confirmed"]) == 3 and history["pending"] == [], "History", history)

        status, by_height, _ = api.call("GET", "/blocks/1")
        status, by_hash, _ = api.call("GET", "/blocks/hash/" + block["hash"])
        status, latest, _ = api.call("GET", "/blocks/latest")
        expect(by_height == by_hash == latest == block, "Block lookups agree", [by_height, by_hash, latest])

        status, page, _ = api.call("GET", "/blocks?limit=1")
        expect(page["total"] == 2 and page["blocks"][0]["height"] == 1 and "transactions" not in page["blocks"][0],
               "Block list desc", page)
        status, page, _ = api.call("GET", "/blocks?order=asc&offset=0&limit=5")
        expect([b["height"] for b in page["blocks"]] == [0, 1], "Block list asc", page)

        status, validation, _ = api.call("GET", "/chain/validate")
        expect(validation == {"valid": True, "height": 1, "error": None}, "Validate", validation)

        # Signing payload for a key-backed sender.
        public_key = "0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798"  # secp256k1 G
        status, payload, _ = api.call("POST", "/transactions/signing-payload",
                                      {"sender": "cc_" + "0" * 64, "recipient": "bob", "amount": 10,
                                       "public_key": public_key})
        expect(status == 200 and payload["payload"].startswith("corecoin-tx-sign-v2|") and
               payload["nonce"] == 0 and payload["algorithm"] == "ECDSA-secp256k1-SHA256", "Signing payload", payload)
        expect_error(api.call("POST", "/transactions", {"sender": "cc_" + "0" * 64, "recipient": "bob",
                                                        "amount": 10, "public_key": public_key,
                                                        "signature": "00" * 64}), 422, "transaction_rejected")

        # Request validation and protocol errors.
        expect_error(api.call("GET", "/blocks/99"), 404, "block_not_found")
        expect_error(api.call("GET", "/blocks/abc"), 400, "invalid_parameter")
        expect_error(api.call("GET", "/blocks?limit=1000"), 400, "invalid_parameter")
        expect_error(api.call("GET", "/transactions/" + "f" * 64), 404, "transaction_not_found")
        expect_error(api.call("GET", "/accounts/bad%20name"), 400, "invalid_address")
        expect_error(api.call("GET", "/nope"), 404, "not_found")
        expect_error(api.call("DELETE", "/blocks"), 405, "method_not_allowed")
        expect_error(api.call("POST", "/blocks", raw=b"{bad", content_type="application/json"), 400, "invalid_json")
        expect_error(api.call("POST", "/blocks", {"miner": "m"}, content_type="text/plain"), 415, "unsupported_media_type")
        expect_error(api.call("POST", "/blocks", {}), 400, "missing_field")
        expect_error(api.call("POST", "/transactions", {"sender": "founder", "recipient": "bob",
                                                        "amount": 1, "amount_core": "1"}), 400, "invalid_amount")
        expect_error(api.call("POST", "/transactions", {"sender": "founder", "recipient": "bob",
                                                        "amount_core": "1.123456789"}), 400, "invalid_amount")
        expect_error(api.call("POST", "/blocks", raw=b"x" * 70000), 413, "payload_too_large")
        expect_error(api.call("POST", "/peers/sync", {"host": "127.0.0.1", "port": unused_port()}),
                     502, "peer_unavailable")
        status, _, headers = api.call("OPTIONS", "/blocks")
        expect(status == 204 and "Access-Control-Allow-Origin" not in headers, "CORS is off by default")
    finally:
        api.stop()

    # Restart: confirmed blocks persist; pending transfers do not.
    api = Api(chain, port)
    try:
        status, node, _ = api.call("GET", "/node")
        expect(node["height"] == 1 and node["pending_transactions"] == 0, "Persisted chain", node)
    finally:
        api.stop()

    # Peer sync through the API from a shell that serves a longer chain.
    remote = Path(directory) / "remote.chain"
    peer_port = unused_port()
    subprocess.run([exe, "--shell", str(remote)], input="mine m1\nmine m2\nquit\n", text=True,
                   capture_output=True, check=True, timeout=60)
    server = subprocess.Popen([exe, "--shell", str(remote)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, text=True)
    server.stdin.write(f"serve 127.0.0.1 {peer_port}\nquit\n")
    server.stdin.flush()
    time.sleep(0.5)
    api = Api(chain, port, "--cors-origin", "http://localhost:3000")
    try:
        result = api.call("POST", "/peers/sync", {"host": "127.0.0.1", "port": peer_port})
        server.communicate(timeout=35)
        # Each node mined its own block 1, so the remote 2-block chain is longer and wins.
        expect(result[0] == 200 and result[1] == {"adopted": True, "height": 2}, "Peer sync", result[1])
        status, _, headers = api.call("GET", "/health", headers={"Origin": "http://localhost:3000"})
        expect(headers["Access-Control-Allow-Origin"] == "http://localhost:3000", "CORS origin", dict(headers))
        status, _, headers = api.call("GET", "/health", headers={"Origin": "http://evil.example"})
        expect("Access-Control-Allow-Origin" not in headers, "CORS rejects other origins", dict(headers))
    finally:
        api.stop()

print("REST API endpoint, persistence, peer sync, and error-handling checks passed")
