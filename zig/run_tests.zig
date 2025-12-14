const std = @import("std");
const utils = @import("utils.zig");

pub fn main() !u8 {
    var allocator = std.heap.GeneralPurposeAllocator(.{}).init;
    const gpa = allocator.allocator();
    var exeFiles = try std.ArrayList([]const u8).initCapacity(gpa, 0);
    defer exeFiles.deinit(gpa);
    try utils.filterFilesInDir(gpa, "./zig/zig-out/tests", &exeFiles, null);
    var retCode: u8 = 0;
    for (exeFiles.items) |cf| {
        var child = std.process.Child.init(&[_][]const u8{exeFiles.items[0]}, gpa);
        try child.spawn();
        const term = try child.wait();
        if (term.Exited != 0) {
            retCode = 1;
        }
        std.debug.print("{s} = exit {}", .{ cf, term.Exited });
    }
    return retCode;
}
