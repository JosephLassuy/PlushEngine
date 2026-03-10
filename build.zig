const std = @import("std");

const cpp_flags = [_][]const u8{
    "-std=c++20",
    "-Wall",
    "-Wextra",
    "-Wpedantic",
};

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const basic_library = b.addLibrary(.{
        .name = "basic_library",
        .linkage = .static,
        .root_module = cppModule(b, target, optimize),
    });
    basic_library.root_module.addIncludePath(b.path("engine/basic_library/include"));
    basic_library.root_module.addCSourceFiles(.{
        .files = &.{
            "engine/basic_library/src/basic_library.cpp",
            "engine/basic_library/src/loop_runner_plugin.cpp",
        },
        .flags = &cpp_flags,
    });
    b.installArtifact(basic_library);

    const engine = addCppExecutable(
        b,
        "PlushEngine",
        "src/main.cpp",
        &.{
            "engine/basic_library/src/graphic/renderer.cpp",
            "engine/basic_library/src/graphic/backend_vulkan.cpp",
        },
        basic_library,
        target,
        optimize,
    );
    linkWindowing(engine.root_module);
    b.installArtifact(engine);

    const flappybird = addCppExecutable(
        b,
        "Flappybird",
        "example/flappybird/main.cpp",
        &.{},
        basic_library,
        target,
        optimize,
    );
    b.installArtifact(flappybird);

    const tutorial = addCppExecutable(
        b,
        "Tutorial",
        "tutorial/main.cpp",
        &.{
            "engine/basic_library/src/graphic/renderer.cpp",
            "engine/basic_library/src/graphic/backend_vulkan.cpp",
        },
        basic_library,
        target,
        optimize,
    );
    linkWindowing(tutorial.root_module);
    b.installArtifact(tutorial);

    addRunStep(b, "run", "Run the main PlushEngine app", engine);
    addRunStep(b, "run-flappybird", "Run the Flappybird example", flappybird);
    addRunStep(b, "run-tutorial", "Run the tutorial cube example", tutorial);

    const shaders_step = b.step("shaders", "Compile GLSL shaders to SPIR-V (requires glslangValidator)");
    const vert_cmd = b.addSystemCommand(&.{ "glslangValidator", "-V", "shaders/cube.vert", "-o", "shaders/cube.vert.spv" });
    const frag_cmd = b.addSystemCommand(&.{ "glslangValidator", "-V", "shaders/cube.frag", "-o", "shaders/cube.frag.spv" });
    shaders_step.dependOn(&vert_cmd.step);
    shaders_step.dependOn(&frag_cmd.step);
}

fn linkWindowing(module: *std.Build.Module) void {
    module.linkSystemLibrary("SDL3", .{
        .use_pkg_config = .force,
    });
    module.linkSystemLibrary("vulkan", .{
        .use_pkg_config = .yes,
    });
}

fn cppModule(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) *std.Build.Module {
    return b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libcpp = true,
    });
}

fn addCppExecutable(
    b: *std.Build,
    name: []const u8,
    source: []const u8,
    extra_sources: []const []const u8,
    basic_library: *std.Build.Step.Compile,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) *std.Build.Step.Compile {
    const exe = b.addExecutable(.{
        .name = name,
        .root_module = cppModule(b, target, optimize),
    });
    exe.root_module.addIncludePath(b.path("engine/basic_library/include"));
    exe.root_module.addIncludePath(b.path("src"));
    exe.root_module.addCSourceFiles(.{
        .files = &.{source},
        .flags = &cpp_flags,
    });
    if (extra_sources.len > 0) {
        exe.root_module.addCSourceFiles(.{
            .files = extra_sources,
            .flags = &cpp_flags,
        });
    }
    exe.root_module.linkLibrary(basic_library);
    return exe;
}

fn addRunStep(
    b: *std.Build,
    step_name: []const u8,
    description: []const u8,
    exe: *std.Build.Step.Compile,
) void {
    const run_cmd = b.addRunArtifact(exe);
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step(step_name, description);
    run_step.dependOn(&run_cmd.step);
}
