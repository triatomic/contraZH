"""Run the drag-to-face rules and formation tests with the local MSVC compiler."""

from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
ENGINE = ROOT / "GeneralsMD/Code/GameEngine"
VCVARS = Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat")

PREAMBLE = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
using std::max;
#include <vector>
typedef int Int;
typedef unsigned int UnsignedInt;
typedef float Real;
typedef bool Bool;
#define RTS_ZEROHOUR 1
#define TRUE true
#define FALSE false
#define CPP_11(x)
#define stricmp _stricmp
#define DEG_TO_RADF(x) ((x) * 0.017453292519943295f)
#define PATHFIND_CELL_SIZE_F 10.0f
#define Cos cosf
#define Sin sinf
struct Coord3D { Real x, y, z; };
struct ICoord2D { Int x, y; };
namespace WWMath { double Round(double x) { return floor(x + 0.5); } }
#define TEST(suite, name) void suite##_##name()
#define EXPECT_TRUE(x) assert(x)
#define EXPECT_FALSE(x) assert(!(x))
#define EXPECT_EQ(a, b) assert((a) == (b))
#define EXPECT_NE(a, b) assert((a) != (b))
#define EXPECT_LT(a, b) assert((a) < (b))
#define EXPECT_LE(a, b) assert((a) <= (b))
#define EXPECT_GT(a, b) assert((a) > (b))
#define EXPECT_GE(a, b) assert((a) >= (b))
#define EXPECT_NEAR(a, b, e) assert(std::fabs((a) - (b)) <= (e))
#define EXPECT_FLOAT_EQ(a, b) EXPECT_NEAR(a, b, 1e-6f)
'''


def header(name, stop=None):
    text = (ENGINE / "Include" / name).read_text()
    if stop:
        text = text[:text.index(stop)]
    return re.sub(r"^#(?:pragma|include)[^\n]*\n", "", text, flags=re.M)


def function(text, name):
    start = text.index(name)
    brace = text.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


def main():
    code = PREAMBLE
    code += header("GameClient/FacingDrag.h", "class FacingDragTranslator")
    code += header("GameLogic/FacingMove.h")
    code += header("GameLogic/LineFormation.h")
    formation = (ENGINE / "Source/GameLogic/AI/LineFormation.cpp").read_text()
    code += formation[formation.index("namespace\n{"):formation.index("Bool isLineFormationMover(")]
    drag = (ENGINE / "Source/GameClient/FacingDragXlat.cpp").read_text()
    for name in ["FacingDragAction facingDragOnButton(", "Bool doesFacingDragOwnButtons(",
                 "Bool isFacingDragLongEnough(", "Real facingAngle(", "Real snapFacingAngle("]:
        code += function(drag, name) + "\n"
    update = (ENGINE / "Source/GameLogic/Object/Update/AIUpdate.cpp").read_text()
    for name in ["Bool shouldFaceOnArrival(",
                 "Bool doesCommandKeepFormationSlot(", "FormationReturn decideFormationReturn("]:
        code += function(update, name) + "\n"
    messages = (ROOT / "Core/GameEngine/Source/Common/MessageStream.cpp").read_text()
    plain_order = function(messages, "Bool GameMessage::isPlainOrder(")
    orders = sorted(set(re.findall(r"GameMessage::(MSG_\w+)", plain_order + drag)))
    code += "class GameMessage { public: enum Type {" + ",".join(orders) + "};" + r'''
    struct Argument { ICoord2D pixel; } argument;
    Type type;
    GameMessage *previous, *following;
    GameMessage(Type t) : type(t), previous(NULL), following(NULL) { argument.pixel.x = argument.pixel.y = 100; }
    ~GameMessage()
    {
        if (previous)
        {
            previous->following = following;
        }
        if (following)
        {
            following->previous = previous;
        }
    }
    Type getType() const { return type; }
    const Argument *getArgument(Int) const { return &argument; }
    GameMessage *prev() const { return previous; }
    GameMessage *next() const { return following; }
    static Bool isPlainOrder(Type);
    };
'''
    code += plain_order + "\n"
    code += (ROOT / "tools/facing_drag/EngineStubs.h").read_text()
    for name in ["GameMessageDisposition FacingDragTranslator::onButton(",
                 "GameMessageDisposition FacingDragTranslator::translateGameMessage(",
                 "void FacingDragTranslator::blockOtherTranslators("]:
        code += function(drag, name) + "\n"
    selection = (ROOT / "Core/GameEngine/Source/GameClient/MessageStream/SelectionXlat.cpp").read_text()
    for name in ["void SelectionTranslator::setDragSelectBlocked(",
                 "GameMessageDisposition SelectionTranslator::onRawMouseLeftButtonDown("]:
        code += function(selection, name) + "\n"
    look = (ROOT / "Core/GameEngine/Source/GameClient/MessageStream/LookAtXlat.cpp").read_text()
    code += function(look, "void LookAtTranslator::setRightMouseScrollBlocked(") + "\n"
    for name in ["void AIUpdateInterface::cancelFacingMove(", "void AIUpdateInterface::releaseFormationSpeed(",
                 "void AIUpdateInterface::setExactFormationMove(", "void AIUpdateInterface::moveToFormationSlot("]:
        code += function(update, name) + "\n"
    command = function(update, "void AIUpdateInterface::aiDoCommand(")
    code += command[:command.index("#ifdef ALLOW_SURRENDER")] + "}\n"
    options = (ENGINE / "Source/GameClient/GUI/GUICallbacks/Menus/OptionsMenu.cpp").read_text()
    code += function(options, "static GameWindow *createFacingDragCheck()") + "\n"
    tests = "\n".join(path.read_text() for path in sorted((ROOT / "tools/facing_drag").glob("*Test.cpp")))
    names = re.findall(r"^TEST\((\w+),\s*(\w+)\)", tests, re.M)
    tests = re.sub(r"(EXPECT_[^\n]+?\))\s*<<[^;]*;", r"\1;", tests)
    code += tests + "\nint main()\n{\n"
    for suite, name in names:
        code += f'    puts("{suite}.{name}"); {suite}_{name}();\n'
    code += f'    puts("Passed {len(names)} drag-to-face tests");\n}}\n'
    with tempfile.TemporaryDirectory(prefix="facing-drag-", dir=ROOT / "build") as directory:
        work = Path(directory)
        (work / "test.cpp").write_text(code, encoding="utf-8")
        (work / "run.bat").write_text(
            f'@echo off\ncall "{VCVARS}" x86 >nul\n'
            'if errorlevel 1 exit /b 1\n'
            'cl /nologo /EHsc /std:c++17 /Fe:test.exe test.cpp\n'
            'if errorlevel 1 exit /b 1\n'
            'test.exe\n', encoding="utf-8")
        subprocess.run(["cmd", "/d", "/c", str(work / "run.bat")], cwd=work, check=True)


if __name__ == "__main__":
    main()
