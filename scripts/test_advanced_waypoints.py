"""Compile and run queue regression checks against the engine's actual methods.

Requires MSVC, clang++, or g++ with a configured C++ standard library. Engine services
are stubbed because the game has no standalone unit-test target.
"""

import argparse
import pathlib
import shutil
import subprocess
import tempfile


ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = ROOT / "Core/GameEngine/Source/GameLogic/System/CommandSequence.cpp"
HEADER = ROOT / "Core/GameEngine/Include/GameLogic/CommandSequence.h"
METHODS = [
    "CommandSequence::CommandSequence(",
    "CommandSequence::~CommandSequence(",
    "CommandSequence::init(",
    "CommandSequence::reset(",
    "CommandSequence::xfer(",
    "CommandSequence::xferCommandNode(",
    "CommandSequence::newCommandNode(",
    "CommandSequence::findCommandByID(",
    "CommandSequence::includeUnitInCommand(",
    "CommandSequence::includeUnitInTypicalMovingCommand(",
    "CommandSequence::isTypicalMovingCommand(",
    "CommandSequence::isEndCommand(",
    "CommandSequence::cleanUpEmptyCommand(",
    "CommandSequence::removeCurrentCommand(",
    "CommandSequence::clearObjectCommand(",
    "CommandSequence::updateConstructionCount(",
    "CommandSequence::removeConstructionShadow(",
    "CommandSequence::getConstructionShadowCount(",
    "CommandSequence::isPendingConstruction(",
]


def extract_method(source, name):
    start = source.rfind("\n", 0, source.index(name)) + 1
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default=shutil.which("cl") or shutil.which("clang++") or shutil.which("g++"))
    args = parser.parse_args()
    if not args.cxx:
        parser.error("Pass --cxx with a C++ compiler path")

    source = SOURCE.read_text(encoding="utf-8")
    header = "\n".join(line for line in HEADER.read_text(encoding="utf-8").splitlines()
                       if not line.startswith(("#include", "#pragma")))
    methods = "\n\n".join(extract_method(source, name) for name in METHODS)
    states = (ROOT / "GeneralsMD/Code/GameEngine/Source/GameLogic/AI/AIStates.cpp").read_text(encoding="utf-8")
    path_methods = "\n\n".join(extract_method(states, name) for name in
                               ["AIStateMachine::xfer(", "AIStateMachine::getPathPointInfo("])
    fixture = (ROOT / "scripts/tests/advanced_waypoint_queue.cpp").read_text(encoding="utf-8")

    with tempfile.TemporaryDirectory(prefix="contra-waypoints-") as directory:
        directory = pathlib.Path(directory)
        (directory / "command_sequence.h").write_text(header, encoding="utf-8")
        (directory / "command_sequence_methods.h").write_text(methods, encoding="utf-8")
        (directory / "path_methods.h").write_text(path_methods, encoding="utf-8")
        test = directory / "test.cpp"
        test.write_text(fixture, encoding="utf-8")
        binary = directory / "test.exe"
        if pathlib.Path(args.cxx).stem.lower() == "cl":
            command = [args.cxx, "/nologo", "/std:c++17", "/EHsc", "/Od", str(test),
                       "/Fe:" + str(binary), "/Fo:" + str(directory / "test.obj")]
        else:
            command = [args.cxx, "-std=c++17", "-O0", "-g", str(test), "-o", str(binary)]
        subprocess.run(command, check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
