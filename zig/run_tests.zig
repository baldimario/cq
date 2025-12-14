const std = @import("std");
const utils = @import("utils.zig");

pub fn main() !u8 {
    var allocator = std.heap.GeneralPurposeAllocator(.{}).init;
    const gpa = allocator.allocator();
    const args = try std.process.argsAlloc(gpa);
    const target = args[1];
    var exeFiles = try std.ArrayList([]const u8).initCapacity(gpa, 0);
    defer exeFiles.deinit(gpa);
    try utils.filterFilesInDir(gpa, try std.fmt.allocPrint(gpa, "./zig/zig-out/tests-{s}", .{target}), &exeFiles, null);
    var retCode: u8 = 0;
    var failTests = try std.ArrayList([]const u8).initCapacity(gpa, 0);
    defer failTests.deinit(gpa);
    for (exeFiles.items) |cf| {
        var child = std.process.Child.init(&[_][]const u8{cf}, gpa);
        try child.spawn();
        const term = try child.wait();
        if (term.Exited != 0) {
            retCode = 1;
            try failTests.append(gpa, std.fs.path.basename(cf));
        }
        utils.print("{s} = exit {}\n", .{ cf, term.Exited }, if (term.Exited == 0) .reset else .red);
    }
    if (failTests.items.len != 0) {
        utils.print("Following file are failing tests \n", .{}, .red);
        for (failTests.items) |it| {
            utils.print("\t{s}\n", .{it}, .red);
        }
    }
    utils.print("Return code: {}\n", .{retCode}, if (retCode == 0) .reset else .red);
    return retCode;
}
