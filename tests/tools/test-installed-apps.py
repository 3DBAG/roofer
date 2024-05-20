import subprocess
from pathlib import Path

import pytest


def print_error_message(output: subprocess.CompletedProcess):
    return f"returned a non-zero exit code, stdout={output.stdout}, stderr={output.stderr}"


@pytest.fixture()
def dir_tests() -> Path:
    """Absolute path to the tests directory"""
    return Path(__file__).parent.parent.absolute()


def test_crop_version():
    """Can we print the crop exe version?"""
    output = subprocess.run(["crop", "--version"], capture_output=True)
    assert output.returncode == 0, print_error_message(output)


def test_crop_wippolder(dir_tests):
    """Can we run crop on the wippolder data?"""
    path_config = dir_tests / "config" / "crop-wippolder.toml"
    output = subprocess.run(["crop", "--config", path_config], capture_output=True)
    assert output.returncode == 0, print_error_message(output)


def test_reconstruct_version():
    """Can we print the reconstruct exe version?"""
    output = subprocess.run(["reconstruct", "--version"], capture_output=True)
    assert output.returncode == 0, print_error_message(output)


def test_reconstruct_wippolder(dir_tests):
    """Can we run reconstruct on the wippolder data?"""
    path_config = dir_tests / "config" / "reconstruct-wippolder.toml"
    output = subprocess.run(["reconstruct", "--config", path_config], capture_output=True)
    assert output.returncode == 0, print_error_message(output)
