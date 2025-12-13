const std = @import("std");
const utils = @import("utils.zig");

pub fn build(b: *std.Build) !void {
    var allocator = std.heap.GeneralPurposeAllocator(.{}).init;
    const gpa = allocator.allocator();
    try utils.buildFor(gpa, .{ .build = b, .artifact_name = "test" });
}
