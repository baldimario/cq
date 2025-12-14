const std = @import("std");
const utils = @import("utils.zig");

pub fn main() !u8 {
    var allocator = std.heap.GeneralPurposeAllocator(.{}).init;
    const gpa = allocator.allocator();
    const args = try std.process.argsAlloc(gpa);
    const target = args[1];
    var exeFiles = try std.ArrayList([]const u8).initCapacity(gpa, 0);
    defer exeFiles.deinit(gpa);
    //const cwdPath = try std.fs.cwd().realpathAlloc(gpa, ".");
    //defer gpa.free(cwdPath);
    //std.debug.print("{s}", .{cwdPath});
    try utils.filterFilesInDir(gpa, try std.fmt.allocPrint(gpa, "./zig/zig-out/tests-{s}", .{target}), &exeFiles, null);
    var retCode: u8 = 0;
    for (exeFiles.items) |cf| {
        var child = std.process.Child.init(&[_][]const u8{cf}, gpa);
        try child.spawn();
        const term = try child.wait();
        if (term.Exited != 0) {
            retCode = 1;
        }
        std.debug.print("{s} = exit {}", .{ cf, term.Exited });
    }
    return retCode;
}
