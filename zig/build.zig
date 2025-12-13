const std = @import("std");
const utils = @import("utils.zig");

pub fn build(b: *std.Build) !void {
    var allocator = std.heap.GeneralPurposeAllocator(.{}).init;
    const gpa = allocator.allocator();
    const trg = b.standardTargetOptions(.{});
    const opt = b.standardOptimizeOption(.{});

    std.debug.print("Building executable", .{});
    try utils.buildFor(gpa, &.{ .target = trg, .optimize = opt, .build = b, .artifact_name = "cq" });

    std.debug.print("Building static library", .{});
    try utils.buildFor(gpa, &.{ .target = trg, .optimize = opt, .build = b, .artifact_name = "cq", .build_lib_info = .{ .excluded_main_src = "main.c", .is_dynamic = false } });

    std.debug.print("Building dynamic library", .{});
    try utils.buildFor(gpa, &.{ .target = trg, .optimize = opt, .build = b, .artifact_name = "cq", .build_lib_info = .{ .excluded_main_src = "main.c", .is_dynamic = true } });
}
