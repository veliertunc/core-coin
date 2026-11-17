"""Exercise wallet commands through the actual interactive CLI."""
import subprocess
import sys
import tempfile
from pathlib import Path

temp = tempfile.TemporaryDirectory()
chain_file = str(Path(temp.name) / "chain.dat")
result = subprocess.run(
    [sys.argv[1], "--shell", chain_file],
    input="\n".join([
        "receive", "balance", "send bob 1000000000", "mine miner",
        "wallet bob", "receive", "balance", "send founder 100000000",
        "mine miner", "balance founder", "balance bob", "validate", "quit", "",
    ]),
    text=True, capture_output=True, check=True,
)
expected = [
    "Selected wallet: founder | 11000.00000000 CORE",
    "Receive address: founder", "Receive address: bob",
    "Confirmed: 10991.00000000 CORE", "Confirmed: 9.00000000 CORE", "> Valid",
]
if "Error:" in result.stdout or result.stdout.count("Queued ") != 2:
    raise RuntimeError(result.stdout + result.stderr)
for text in expected:
    if text not in result.stdout:
        raise RuntimeError(f"Missing {text!r}:\n{result.stdout}")
print("CLI wallet send/receive round trip passed")

reopened = subprocess.run([sys.argv[1], "--shell", chain_file], input="balance founder\nbalance bob\nvalidate\nquit\n", text=True, capture_output=True, check=True)
for value in ["Confirmed: 10991.00000000 CORE", "Confirmed: 9.00000000 CORE", "> Valid"]:
    if value not in reopened.stdout:
        raise RuntimeError(reopened.stdout + reopened.stderr)
with open(chain_file, "a", encoding="utf-8") as saved:
    saved.write("tampered")
invalid = subprocess.run([sys.argv[1], "--shell", chain_file], input="quit\n", text=True, capture_output=True)
if invalid.returncode == 0:
    raise RuntimeError("Corrupt chain file accepted")
print("Chain restart and corruption checks passed")
