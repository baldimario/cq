const std = @import("std");

/// custom build info
pub const BuildInfo = struct {
    /// build ref
    build: *std.Build,

    /// (optional) target requested
    target: std.Build.ResolvedTarget,

    optimize: std.builtin.OptimizeMode,

    /// name of artifact to build
    artifact_name: []const u8,

    /// additional headers files
    headers: ?[][]const u8 = null,

    /// Info for build a lib. If null, an executable will be build
    build_lib_info: ?struct {
        /// true if the lib has to be dynamically linked, else will be statically
        is_dynamic: bool,

        /// the file to ignore (usually containing the main function). If null, none files will be ignored
        excluded_main_src: ?[]const u8 = null,
    } = null,

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

pub fn buildFor(allocator: std.mem.Allocator, info: *const BuildInfo) !void {
    const b = info.build;
    const target = info.target;

    const optimize = info.optimize;

    const module = b.createModule(.{ .link_libc = true, .optimize = optimize, .target = target });
    const isLib = info.build_lib_info != null;

    const exe = switch (isLib) {
        false => b.addExecutable(.{ .name = info.artifact_name, .root_module = module }),
        true => b.addLibrary(.{ .name = info.artifact_name, .root_module = module, .linkage = if (info.build_lib_info.?.is_dynamic) .dynamic else .static }),
    };
    const excludedMainSrcIfLib = if (isLib) info.build_lib_info.?.excluded_main_src else null;
    module.addIncludePath(b.path("./../include"));

    // handle addition header files
    if (info.headers != null) {
        for (info.headers.?) |ch| {
            module.addIncludePath(b.path(ch));
        }
    }

    try addCFilesFromDir(b, exe, "../src", excludedMainSrcIfLib);

    // TODO: resolve sub-directory at more level too
    try addCFilesFromDir(b, exe, "../src/external", excludedMainSrcIfLib);
    const postifix = if (!isLib) "" else switch (info.build_lib_info.?.is_dynamic) {
        true => "_shared",
        false => "_lib",
    };
    const install_prefix = try std.fmt.allocPrint(allocator, "{s}-{s}{s}", .{ @tagName(target.result.cpu.arch), @tagName(target.result.os.tag), postifix });
    const art = b.addInstallArtifact(exe, .{});
    art.dest_dir = .{ .custom = install_prefix };
    b.getInstallStep().dependOn(&art.step);
}

pub fn addCFilesFromDir(b: *std.Build, exe: *std.Build.Step.Compile, dir_path: []const u8, exclude_path: ?[]const u8) !void {
    var dir = try std.fs.cwd().openDir(dir_path, .{ .iterate = true });
    defer dir.close();
    const fullExcludedPath = if (exclude_path != null) try std.fmt.allocPrint(b.allocator, "{s}/{s}", .{ dir_path, exclude_path.? }) else null;

    var it = dir.iterate();
    while (try it.next()) |entry| {
        if (entry.kind == .file and std.mem.endsWith(u8, entry.name, ".c")) {
            const full_path = try std.fmt.allocPrint(b.allocator, "{s}/{s}", .{ dir_path, entry.name });
            defer b.allocator.free(full_path);
            if (fullExcludedPath != null and std.mem.eql(u8, fullExcludedPath.?, full_path)) {
                std.debug.print("Found {s}: ignored\n", .{full_path});
                continue;
            }
            std.debug.print("added source: {s}\n", .{full_path});
            exe.addCSourceFile(.{
                .file = b.path(full_path),
                .flags = &.{},
            });
        }
    }
    if (fullExcludedPath != null) {
        defer b.allocator.free(fullExcludedPath.?);
    }
}
