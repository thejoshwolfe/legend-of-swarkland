const std = @import("std");
const build_options = @import("build_options");
const display_main = @import("client").display_main;

pub fn main() void {
    if (build_options.headless) return;
    display_main();
}
