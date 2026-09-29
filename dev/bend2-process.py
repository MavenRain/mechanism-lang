"""Bound commands, including cooperative main-thread helper nesting."""
import os
import signal
import subprocess
import threading
import time


# This private candidate reserves a child-environment key for cleanup order.
# Four helper layers get TERM grace 2, 1, .5 and .25 seconds respectively.
# Deeper layers remain bounded but are outside the tested cleanup guarantee.
_DEPTH = "__BEND2_PROCESS_CLEANUP_DEPTH"
_DRAIN_GRACE = 0.1


def run(args, *, timeout=None, capture_output=False, check=False, input=None, **kwargs):
    if capture_output:
        if "stdout" in kwargs or "stderr" in kwargs:
            raise ValueError("stdout/stderr cannot accompany capture_output")
        kwargs.update(stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if input is not None:
        if "stdin" in kwargs:
            raise ValueError("stdin cannot accompany input")
        kwargs["stdin"] = subprocess.PIPE
    if "start_new_session" in kwargs:
        raise ValueError("bounded commands own their process session")
    try:
        depth = max(0, int(os.environ.get(_DEPTH, "0")))
    except ValueError:
        depth = 0
    environment = dict(os.environ if kwargs.get("env") is None else kwargs["env"])
    environment.pop(_DEPTH, None)
    environment.pop(os.fsencode(_DEPTH), None)
    environment[_DEPTH] = str(depth + 1)
    kwargs["env"] = environment
    grace = 2.0 / (2 ** min(depth, 3))
    process = subprocess.Popen(args, start_new_session=True, **kwargs)
    handlers = {}
    cleaning = False
    drained = (None, None)

    def signal_group(signum):
        try:
            os.killpg(process.pid, signum)
        except ProcessLookupError:
            pass
        except PermissionError:
            # Darwin may refuse a signal to a group containing only zombies.
            # Suppress that refusal only after the kernel confirms absence.
            if process.poll() is None:
                raise
            deadline = time.monotonic() + _DRAIN_GRACE
            while time.monotonic() < deadline:
                try:
                    os.killpg(process.pid, 0)
                except ProcessLookupError:
                    return
                except PermissionError:
                    pass
                time.sleep(0.005)
            raise

    def partial(value):
        if process.text_mode and isinstance(value, bytes):
            return value.decode(process.encoding, process.errors)
        return value

    def cleanup():
        nonlocal cleaning, drained
        if cleaning:
            return drained
        cleaning = True
        signal_group(signal.SIGTERM)
        try:
            drained = process.communicate(timeout=grace)
        except subprocess.TimeoutExpired:
            pass
        finally:
            signal_group(signal.SIGKILL)
        try:
            drained = process.communicate(timeout=_DRAIN_GRACE)
        except subprocess.TimeoutExpired as error:
            # Escaped descendants must not hold captured pipes open forever.
            drained = (partial(error.output), partial(error.stderr))
            for stream in (process.stdin, process.stdout, process.stderr):
                if stream is not None:
                    stream.close()
            try:
                process.wait(timeout=_DRAIN_GRACE)
            except subprocess.TimeoutExpired:
                # Keep the original timeout or interruption verdict bounded.
                pass
        return drained

    def interrupted(signum, frame):
        if cleaning:
            return
        cleanup()
        raise SystemExit(128 + signum)

    try:
        if threading.current_thread() is threading.main_thread():
            for signum in (signal.SIGINT, signal.SIGTERM):
                handlers[signum] = signal.signal(signum, interrupted)
        try:
            stdout, stderr = process.communicate(input, timeout=timeout)
        except subprocess.TimeoutExpired:
            stdout, stderr = cleanup()
            raise subprocess.TimeoutExpired(args, timeout, output=stdout, stderr=stderr)
        if check and process.returncode:
            raise subprocess.CalledProcessError(process.returncode, args, stdout, stderr)
        return subprocess.CompletedProcess(args, process.returncode, stdout, stderr)
    except BaseException:
        cleanup()
        raise
    finally:
        for signum, handler in handlers.items():
            signal.signal(signum, handler)
