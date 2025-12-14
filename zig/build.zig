const std = @import("std");
const utils = @import("utils.zig");
const tests = @import("build_tests.zig");

pub fn build(b: *std.Build) !void {
    var allocator = std.heap.GeneralPurposeAllocator(.{}).init;
    const gpa = allocator.allocator();
    const trg = b.standardTargetOptions(.{});
    const opt = b.standardOptimizeOption(.{});

    std.debug.print("Building executable", .{});
    _ = try utils.buildFor(gpa, &.{ .target = trg, .optimize = opt, .build = b, .artifact_name = "cq" });

    std.debug.print("Building static library", .{});
    const libStep = try utils.buildFor(gpa, &.{ .target = trg, .optimize = opt, .build = b, .artifact_name = "cq", .build_lib_info = .{ .excluded_main_src = "main.c", .is_dynamic = false } });

    std.debug.print("Building dynamic library", .{});
    _ = try utils.buildFor(gpa, &.{ .target = trg, .optimize = opt, .build = b, .artifact_name = "cq", .build_lib_info = .{ .excluded_main_src = "main.c", .is_dynamic = true } });

    std.debug.print("Building dynamic library", .{});
    _ = try tests.buildTests(b, libStep, trg, opt);
}
