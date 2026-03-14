from __future__ import annotations

import subprocess
from pathlib import Path
from typing import Mapping


def start_simulator_process(
    binary: Path,
    *args: str,
    config_file: Path | None = None,
    env: Mapping[str, str] | None = None,
    stdout: int | None = subprocess.PIPE,
    stderr: int | None = subprocess.PIPE,
) -> subprocess.Popen[str]:
    command = [str(binary), *args]
    if config_file is not None:
        command.extend(["--config-file", str(config_file)])

    return subprocess.Popen(
        command,
        cwd=binary.parent,
        stdin=subprocess.DEVNULL,
        stdout=stdout,
        stderr=stderr,
        text=True,
        env=dict(env) if env is not None else None,
    )


def communicate_with_timeout(
    proc: subprocess.Popen[str],
    timeout_seconds: float,
    terminate_timeout_seconds: float = 3.0,
) -> tuple[str, str]:
    try:
        stdout_output, stderr_output = proc.communicate(timeout=timeout_seconds)
    except subprocess.TimeoutExpired:
        proc.terminate()
        try:
            stdout_output, stderr_output = proc.communicate(
                timeout=terminate_timeout_seconds
            )
        except subprocess.TimeoutExpired:
            proc.kill()
            stdout_output, stderr_output = proc.communicate(
                timeout=terminate_timeout_seconds
            )

    return (stdout_output or "", stderr_output or "")


def terminate_process(
    proc: subprocess.Popen[str], timeout_seconds: float = 3.0
) -> None:
    if proc.poll() is not None:
        return

    proc.terminate()
    try:
        proc.wait(timeout=timeout_seconds)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait(timeout=timeout_seconds)
