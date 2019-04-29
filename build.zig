const std = @import("std");
const builtin = @import("builtin");
const Builder = std.build.Builder;

pub fn build(b: *Builder) void {
    const lazer = b.addExecutable("lazer", "src/main.zig");
    lazer.setBuildMode(getBuildMode(b));
    lazer.setMainPkgPath("src/");

    // only support windows & linux x86_64 for now
    if (builtin.os != builtin.Os.windows and builtin.os != builtin.Os.linux) {
        @compileError("Operating system not supported");
    } else if (builtin.arch != builtin.Arch.x86_64) {
        @compileError("Architecture not supported");
    } else {
        lazer.setTarget(builtin.arch, builtin.os, builtin.Abi.gnu);
    }

    b.default_step.dependOn(&lazer.step);
    b.installArtifact(lazer);
    
    // for running lazer sometimes when testing
    const run = b.step("run", "Run to test the lazer executable");
    run.dependOn(&lazer.run().step);

    // for executing actual test cases
    createTests(b);
}

fn getBuildMode(b: *Builder) builtin.Mode {
    const release = b.option(bool, "release", "Speed optimized binary") orelse false;
    const small = b.option(bool, "small", "Size optimized binary") orelse false;

    return (
        if (release) builtin.Mode.ReleaseFast
        else if (small) builtin.Mode.ReleaseSmall
        else builtin.Mode.Debug
    );
}

fn createTests(b: *Builder) void {
    const test_files = [][]const u8 {
        "src/platform/heap.zig",
        "src/platform/memory.zig",

        "src/runtime/atom.zig",
    };

    const tests = b.step("test", "Run all the tests");
    for (test_files) |test_file| {
        const test_step = b.addTest(test_file);
        test_step.setMainPkgPath("src/");
        tests.dependOn(&test_step.step);
    }
}