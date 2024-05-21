import platform
import subprocess
from pathlib import Path

import pytest


def print_error_message(output: subprocess.CompletedProcess):
    return f"returned a non-zero exit code, stdout={output.stdout}, stderr={output.stderr}"


@pytest.fixture()
def dir_tests() -> Path:
    """Absolute path to the tests directory."""
    return Path(__file__).parent.parent.absolute()


@pytest.fixture()
def crop_exe() -> str:
    """Crop executable, OS-dependent."""
    if platform.system() == 'Windows':
        return 'crop.exe'
    else:
        return 'crop'


@pytest.fixture()
def reconstruct_exe() -> str:
    """Reconstruct executable, OS-dependent."""
    if platform.system() == 'Windows':
        return 'reconstruct.exe'
    else:
        return 'reconstruct'


def test_crop_version(crop_exe):
    """Can we print the crop exe version?"""
    output = subprocess.run([crop_exe, "--version"], capture_output=True)
    assert output.returncode == 0, print_error_message(output)


def test_crop_wippolder(crop_exe, dir_tests):
    """Can we run crop on the wippolder data?"""
    path_config = dir_tests / "config" / "crop-wippolder.toml"
    output = subprocess.run([crop_exe, "--config", path_config], capture_output=True)
    assert output.returncode == 0, print_error_message(output)


def test_reconstruct_version(reconstruct_exe):
    """Can we print the reconstruct exe version?"""
    output = subprocess.run([reconstruct_exe, "--version"], capture_output=True)
    assert output.returncode == 0, print_error_message(output)


def test_reconstruct_wippolder(reconstruct_exe, dir_tests):
    """Can we run reconstruct on the wippolder data?"""
    path_config = dir_tests / "config" / "reconstruct-wippolder.toml"
    output = subprocess.run([reconstruct_exe, "--config", path_config], capture_output=True)
    assert output.returncode == 0, print_error_message(output)
