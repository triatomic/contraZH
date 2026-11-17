"""Run isolated turret scan checks from an x86 MSVC developer prompt.

Usage: python tools/turret_scan_regression.py
The harness extracts the production turn and scan methods. It stubs the owner,
pitch, and turret data, and uses standard-library scalar math. It does not run
the game, rendering, sounds, or the surrounding state machine.
"""

import argparse
from pathlib import Path
import subprocess
import tempfile


def function(root, path, signature):
    source = (root / path).read_text(encoding="cp1252")
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


HARNESS = r'''
#include <cmath>
#include <cstdio>
typedef float Real;
typedef bool Bool;
#define PI 3.14159265359f
#define TWO_PI 6.28318530718f
#define WWMATH_PI PI
#define WWMATH_TWO_PI TWO_PI
#define WWINLINE inline
#define _isnan std::isnan
#define DEBUG_ASSERTCRASH(c, m) ((void)0)
#if defined(GENERALS_ONLINE_HIGH_FPS_SERVER)
#define TEST_LOGIC_FPS 60
#else
#define TEST_LOGIC_FPS 30
#endif
enum { MODELCONDITION_TURRET_ROTATE, OBJECT_STATUS_UNDER_CONSTRUCTION };
enum StateReturnType { STATE_SUCCESS = -1, STATE_CONTINUE = 0 };
struct WWMath
{
    static float Normalize_Angle(float angle);
    static float Floorf(float angle) { return floorf(angle); }
    static float Fabs(float angle) { return fabsf(angle); }
};
struct Owner
{
    bool rotating, underConstruction;
    Owner() : rotating(false), underConstruction(false) {}
    void clearModelConditionState(int) { rotating = false; }
    void setModelConditionState(int) { rotating = true; }
    void reactToTurretChange(int, float, float) {}
    bool testStatus(int) { return underConstruction; }
};
struct TurretAI
{
    float m_angle, m_pitch, rate, naturalAngle;
    int m_whichTurret;
    bool m_playRotSound, pitchAligned;
    Owner owner;
    TurretAI() : m_angle(0), m_pitch(0), rate(0), naturalAngle(0),
        m_whichTurret(0), m_playRotSound(false), pitchAligned(true) {}
    float getTurretAngle() { return m_angle; }
    float getTurnRate() { return rate; }
    float getMinTurretAngle() { return 0; }
    float getMaxTurretAngle() { return 0; }
    bool hasLimitedTurretAngle() { return false; }
    bool getTurretRotationDir(float, float, float) { return false; }
    Owner* getOwner() { return &owner; }
    float getNaturalTurretAngle() { return naturalAngle; }
    float getNaturalTurretPitch() { return 0; }
    bool friend_turnTowardsPitch(float, float) { return pitchAligned; }
    bool friend_turnTowardsAngle(float desired, float modifier, float threshold);
};
struct TurretAIIdleScanState
{
    TurretAI* turret;
    float m_desiredAngle;
    TurretAIIdleScanState(TurretAI* value) : turret(value), m_desiredAngle(0) {}
    TurretAI* getTurretAI() { return turret; }
    Owner* getMachineOwner() { return &turret->owner; }
    StateReturnType update();
};
'''

CHECKS = r'''
int main()
{
    const char* names[] = { "Leonidas", "Technical", "Guardian", "negative", "wrap" };
    float rates[] = { 60, 240, 270, 60, 270 };
    float goals[] = { 20.3f, 45.3f, 120.3f, -29.7f, 350.3f };
    for (int unit = 0; unit < 5; ++unit)
    {
        TurretAI turret;
        turret.rate = rates[unit] * PI / 180 / TEST_LOGIC_FPS;
        TurretAIIdleScanState scan(&turret);
        scan.m_desiredAngle = goals[unit] * PI / 180;
        int frames = 0;
        while (scan.update() != STATE_SUCCESS && ++frames < 600)
        {
        }
        float error = WWMath::Fabs(stdAngleDiffMod(turret.m_angle, normalizeAngle(scan.m_desiredAngle)));
        if (frames >= 600 || error > 0.00001f || turret.owner.rotating)
        {
            printf("FAIL %s requested=%g actual=%g rotating=%d\n", names[unit], goals[unit], turret.m_angle * 180 / PI, turret.owner.rotating);
            return 1;
        }
    }
    TurretAI exact;
    exact.rate = 240 * PI / 180 / TEST_LOGIC_FPS;
    float step = exact.rate * 0.5f;
#if defined(GENERALS_ONLINE_HIGH_FPS_SERVER)
    step *= 2.0f;
#endif
    if (!exact.friend_turnTowardsAngle(step, 0.5f, 0) || exact.owner.rotating)
    {
        puts("FAIL exact-step turn retains the rotation condition");
        return 2;
    }
    exact.m_angle = step;
    if (!exact.friend_turnTowardsAngle(0, 0.5f, 0) || exact.owner.rotating)
    {
        puts("FAIL exact-step recenter retains the rotation condition");
        return 3;
    }
    exact.m_angle = 0;
    if (!exact.friend_turnTowardsAngle(step * 2, 0.5f, step * 1.5f) || !exact.owner.rotating)
    {
        puts("FAIL combat aiming no longer preserves its nonzero tolerance");
        return 4;
    }
    TurretAIIdleScanState scan(&exact);
    exact.m_angle = 0;
    exact.owner.rotating = false;
    exact.owner.underConstruction = true;
    scan.m_desiredAngle = 1;
    if (scan.update() != STATE_CONTINUE || exact.m_angle != 0 || exact.owner.rotating)
    {
        puts("FAIL turret scans during construction");
        return 5;
    }
    exact.owner.underConstruction = false;
    exact.pitchAligned = false;
    scan.m_desiredAngle = 0;
    if (scan.update() != STATE_CONTINUE)
    {
        puts("FAIL scan completes before pitch alignment");
        return 6;
    }
    puts("PASS scan angles, rotation cleanup, recentering, aim tolerance, construction, pitch");
    return 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--compiler", default="cl")
    parser.add_argument("--mode", choices=("all", "offline", "go30", "go60"), default="all")
    args = parser.parse_args()
    root = args.source_root.resolve()
    turret = "GeneralsMD/Code/GameEngine/Source/GameLogic/AI/TurretAI.cpp"
    common = "Core/GameEngine/Source/Common/System/GameCommon.cpp"
    bodies = [
        function(root, "Core/Libraries/Include/Lib/BaseType.h", "inline Real nmod("),
        function(root, "Core/Libraries/Source/WWVegas/WWMath/wwmath.h", "WWINLINE float WWMath::Normalize_Angle("),
        function(root, common, "Real normalizeAngle("),
        function(root, common, "extern Real stdAngleDiffMod("),
        function(root, turret, "Bool TurretAI::friend_turnTowardsAngle("),
        function(root, turret, "StateReturnType TurretAIIdleScanState::update("),
    ]
    with tempfile.TemporaryDirectory(prefix="turret-scan-") as directory:
        work = Path(directory)
        source = work / "turret_scan.cpp"
        source.write_text(HARNESS + "\n".join(bodies) + CHECKS, encoding="cp1252")
        for mode, defines in (
            ("offline", []),
            ("go30", ["GENERALS_ONLINE"]),
            ("go60", ["GENERALS_ONLINE", "GENERALS_ONLINE_HIGH_FPS_SERVER"]),
        ):
            if args.mode not in ("all", mode):
                continue
            for cheats in (False, True):
                flags = defines + (["_ALLOW_DEBUG_CHEATS_IN_RELEASE"] if cheats else [])
                print(f"{mode}, cheats={'on' if cheats else 'off'}", flush=True)
                command = [args.compiler, "/nologo", "/EHsc", "/O2"]
                command += ["/D" + flag for flag in flags]
                command += [str(source), "/Fe:" + str(work / "turret_scan.exe")]
                subprocess.run(command, cwd=work, check=True)
                subprocess.run([str(work / "turret_scan.exe")], cwd=work, check=True)


if __name__ == "__main__":
    main()
