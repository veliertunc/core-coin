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

# Exercise decimal sends and both pending and confirmed history, then restart.
flow_file = str(Path(temp.name) / "flow.dat")
def shell(path, commands):
    return subprocess.run([sys.argv[1], "--shell", path], input=commands, text=True, capture_output=True)
flow = shell(flow_file, "send-core bob 1.00000001\nhistory bob\nmine miner\nhistory bob\nsend-core bob 0.1\nquit\n")
for value in ["Pending ", "Confirmed block 1 ", "1.00000001 CORE"]:
    if flow.returncode or value not in flow.stdout:
        raise RuntimeError(flow.stdout + flow.stderr)
reopen = shell(flow_file, "balance bob\nhistory bob\nquit\n")
if reopen.returncode or "Confirmed: 1.00000001 CORE" not in reopen.stdout or "Pending " in reopen.stdout:
    raise RuntimeError(reopen.stdout + reopen.stderr)
# Simulate interruption after the prior chain was renamed to its backup.
flow_path = Path(flow_file)
backup = Path(flow_file + ".bak")
flow_path.rename(backup)
recovered = shell(flow_file, "balance bob\nquit\n")
if recovered.returncode or "Confirmed: 1.00000001 CORE" not in recovered.stdout or backup.exists():
    raise RuntimeError(recovered.stdout + recovered.stderr)
# An invalid backup must not be promoted or replaced by a new genesis.
flow_path.rename(backup)
backup.write_text("truncated", encoding="utf-8")
invalid_backup = shell(flow_file, "quit\n")
if invalid_backup.returncode == 0 or flow_path.exists() or not backup.exists():
    raise RuntimeError("Invalid backup was accepted or overwritten")
# A failed save leaves the last committed chain intact.
failed_file = Path(temp.name) / "failed.dat"
initial = shell(str(failed_file), "quit\n")
if initial.returncode:
    raise RuntimeError(initial.stderr)
(failed_file.parent / (failed_file.name + ".tmp")).mkdir()
failed = shell(str(failed_file), "send-core bob 1\nmine miner\nquit\n")
if "Error:" not in failed.stdout:
    raise RuntimeError("Expected failed save")
after = shell(str(failed_file), "balance bob\nquit\n")
if after.returncode or "Confirmed: 0.00000000 CORE" not in after.stdout:
    raise RuntimeError(after.stdout + after.stderr)
print("Decimal history, pending restart, and save recovery checks passed")
