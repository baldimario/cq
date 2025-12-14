const std = @import("std");
const utils = @import("utils.zig");

pub fn buildTests(b: *std.Build, libStep: *std.Build.Step.Compile, target: std.Build.ResolvedTarget, optimize: std.builtin.OptimizeMode) !void {
    var allocator = std.heap.GeneralPurposeAllocator(.{}).init;
    const gpa = allocator.allocator();
    var headers = try std.ArrayList([]const u8).initCapacity(gpa, 0);
    var srcs = try std.ArrayList([]const u8).initCapacity(gpa, 0);
    try utils.getFilesInDir(gpa, "../tests", &srcs, &headers);
    defer headers.deinit(gpa);
    defer srcs.deinit(gpa);

    for (headers.items) |cf| {
        std.debug.print("{s}\n", .{cf});
    }

    for (srcs.items) |cf| {
        std.debug.print("{s}\n", .{cf});
        const module = b.createModule(.{
            .link_libc = true,
            .optimize = optimize,
            .target = target,
        });
        module.addIncludePath(b.path("./../include"));

        const exe = b.addExecutable(.{ .name = std.fs.path.basename(b.path(cf).getPath(b)), .root_module = module });
        for (headers.items) |headerFp| {
            module.addIncludePath(b.path(headerFp));
            exe.addCSourceFile(.{
                .file = b.path(cf),
                .flags = &.{},
            });
            exe.root_module.linkLibrary(libStep);
            const art = b.addInstallArtifact(exe, .{});
            art.dest_dir = .{ .custom = "tests" };
            b.getInstallStep().dependOn(&art.step);
        }
    }
}
