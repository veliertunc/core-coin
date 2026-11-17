"""Run two local nodes and test sync, rejection, and malformed peer responses."""
import socket
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

exe = sys.argv[1]

def unused_port():
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        return listener.getsockname()[1]

def run_shell(chain, commands):
    return subprocess.run([exe, "--shell", str(chain)], input=commands, capture_output=True,
                          text=True, timeout=40, check=True)

def run_server(chain, port, commands=""):
    process = subprocess.Popen([exe, "--shell", str(chain)], stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, bufsize=1)
    ready = threading.Event()
    output = []
    def read_output():
        for line in process.stdout:
            output.append(line)
            if "Serving one peer request" in line:
                ready.set()
    reader = threading.Thread(target=read_output, daemon=True)
    reader.start()
    process.stdin.write(commands + f"serve 127.0.0.1 {port}\nquit\n")
    process.stdin.close()
    if not ready.wait(15):
        process.kill()
        raise RuntimeError("Peer server did not start: " + "".join(output))
    # The message is emitted immediately before bind; allow the listener to start.
    time.sleep(0.25)
    return process, reader, output

def finish_server(process, reader, output):
    try:
        process.wait(timeout=35)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()
        raise RuntimeError("Peer server timed out")
    reader.join(timeout=2)
    if process.returncode or "Peer request served" not in "".join(output):
        raise RuntimeError("Peer server failed: " + "".join(output) + process.stderr.read())

with tempfile.TemporaryDirectory() as directory:
    remote, local = Path(directory) / "remote.chain", Path(directory) / "local.chain"
    port = unused_port()
    server, reader, output = run_server(remote, port, "send-core bob 2\nmine miner\n")
    client = run_shell(local, f"sync 127.0.0.1 {port}\nbalance bob\nvalidate\nquit\n")
    finish_server(server, reader, output)
    if "Adopted peer chain at height 1" not in client.stdout or "Confirmed: 2.00000000 CORE" not in client.stdout or "> Valid" not in client.stdout:
        raise RuntimeError(client.stdout + client.stderr)
    reopened = run_shell(local, "balance bob\nquit\n")
    if "Confirmed: 2.00000000 CORE" not in reopened.stdout:
        raise RuntimeError(reopened.stdout)

    # An equal-height peer is not adopted.
    port = unused_port()
    server, reader, output = run_server(remote, port)
    tied = run_shell(local, f"sync 127.0.0.1 {port}\nquit\n")
    finish_server(server, reader, output)
    if "Kept current chain" not in tied.stdout:
        raise RuntimeError(tied.stdout)

    # A local pending transfer is preserved when a longer chain is received.
    port = unused_port()
    server, reader, output = run_server(remote, port, "mine miner\n")
    pending = run_shell(local, f"send-core bob 1\nsync 127.0.0.1 {port}\nbalance bob\nquit\n")
    finish_server(server, reader, output)
    if "Cannot replace chain while transactions are pending" not in pending.stdout or "Including pending: 3.00000000 CORE" not in pending.stdout:
        raise RuntimeError(pending.stdout)

    # A malicious length is rejected before allocating or parsing its payload.
    port = unused_port()
    ready = threading.Event()
    def malicious():
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", port))
            listener.listen(1)
            ready.set()
            with listener.accept()[0] as peer:
                peer.recv(4)
                peer.sendall(b"\xff\xff\xff\xff")
    thread = threading.Thread(target=malicious, daemon=True)
    thread.start()
    if not ready.wait(5):
        raise RuntimeError("Malicious peer did not start")
    rejected = run_shell(local, f"sync 127.0.0.1 {port}\nbalance bob\nquit\n")
    thread.join(timeout=5)
    if "Invalid peer response size" not in rejected.stdout or "Confirmed: 2.00000000 CORE" not in rejected.stdout:
        raise RuntimeError(rejected.stdout)
print("Peer sync, fork choice, pending preservation, and malformed response checks passed")
