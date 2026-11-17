"""Exercise wallet commands through the actual interactive CLI."""
import subprocess
import sys

result = subprocess.run(
    [sys.argv[1], "--shell"],
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
