const std = @import("std");

fn buildFor(b: *std.Build, trg: ?std.Target.Query, allocator: std.mem.Allocator) !void {
    const target = if (trg == null) b.standardTargetOptions(.{}) else b.standardTargetOptions(.{ .default_target = trg.? });
    const optimize = b.standardOptimizeOption(.{});

    const module = b.createModule(.{ .link_libc = true, .optimize = optimize, .target = target });

    const exe = b.addExecutable(.{ .name = "cq", .root_module = module });
    module.addIncludePath(b.path("./../include"));
    try addCFilesFromDir(b, exe, "../src");
    // TODO: resolve sub-directory at more level too
    try addCFilesFromDir(b, exe, "../src/external/");
    const install_prefix = try std.fmt.allocPrint(allocator, "{}-{}", .{ target.result.os.tag, target.result.cpu.arch });
    std.debug.print("{s}", .{install_prefix});
    //defer allocator.free(install_prefix);
    const art = b.addInstallArtifact(exe, .{});
    art.dest_dir = .{ .custom = install_prefix };
    //b.installArtifact(exe);
    b.getInstallStep().dependOn(&art.step);
}

pub fn build(b: *std.Build) !void {
    var allocator = std.heap.GeneralPurposeAllocator(.{}).init;
    const gpa = allocator.allocator();
    try buildFor(b, null, gpa);
}

fn addCFilesFromDir(
    b: *std.Build,
    exe: *std.Build.Step.Compile,
    dir_path: []const u8,
) !void {
    var dir = try std.fs.cwd().openDir(dir_path, .{ .iterate = true });
    defer dir.close();

    var it = dir.iterate();
    while (try it.next()) |entry| {
        if (entry.kind == .file and std.mem.endsWith(u8, entry.name, ".c")) {
            const full_path = try std.fmt.allocPrint(b.allocator, "{s}/{s}", .{ dir_path, entry.name });
            defer b.allocator.free(full_path);
            exe.addCSourceFile(.{
                .file = b.path(full_path),
                .flags = &.{}, // oppure &.{"-Wall"} se vuoi
            });
        }
    }
}
