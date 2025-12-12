const std = @import("std");

/// custom build info
pub const BuildInfo = struct {
    /// build ref
    build: *std.Build,

    /// (optional) target requested
    target: ?std.Target.Query = null,

    /// name of artifact to build
    artifact_name: []const u8,

    /// additional headers files
    headers: ?[][]const u8 = null,

    exclude_main: bool = false,

    /// all src used for build. If null, the defaul src directory (from c project) is used
    srcs: ?struct {
        /// Directory path (relative to cwd) where source will be found
        directory: []const u8,

        /// If only one file is needed, the file (relative to directory set with same field name)
        file: ?[]const u8,
    } = null,

    /// custom out settings. If null, the generation wil be in directory with os-arch pattern
    out: ?struct { prefix: []const u8 } = null,
};

pub fn buildFor(allocator: std.mem.Allocator, info: BuildInfo) !void {
    const b = info.build;
    const target = if (info.target == null) b.standardTargetOptions(.{}) else b.standardTargetOptions(.{ .default_target = info.target.? });
    const optimize = b.standardOptimizeOption(.{});

    const module = b.createModule(.{ .link_libc = true, .optimize = optimize, .target = target });

    const exe = b.addExecutable(.{ .name = info.artifact_name, .root_module = module });
    module.addIncludePath(b.path("./../include"));

    // handle addition header files
    if (info.headers != null) {
        for (info.headers.?) |ch| {
            module.addIncludePath(b.path(ch));
        }
    }
    try addCFilesFromDir(b, exe, "../src");

    // TODO: resolve sub-directory at more level too
    try addCFilesFromDir(b, exe, "../src/external/");
    const install_prefix = try std.fmt.allocPrint(allocator, "{s}-{s}", .{ @tagName(target.result.os.tag), @tagName(target.result.cpu.arch) });
    const art = b.addInstallArtifact(exe, .{});
    art.dest_dir = .{ .custom = install_prefix };
    b.getInstallStep().dependOn(&art.step);
}

pub fn addCFilesFromDir(
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
                .flags = &.{},
            });
        }
    }
}
