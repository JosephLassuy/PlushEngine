const std = @import("std");

// ---------- Math (ported from C++) ----------

/// Column-major 4x4 matrix (OpenGL/Vulkan convention)
const Mat4 = struct {
    m: [16]f32,
};

fn perspective(fov_deg: f32, aspect: f32, near_z: f32, far_z: f32) Mat4 {
    const pi: f32 = 3.14159265;
    const f: f32 = 1.0 / std.math.tan(f32, fov_deg * pi / 360.0);
    const nf: f32 = 1.0 / (near_z - far_z);

    var p = Mat4{ .m = .{0} ** 16 };
    p.m[0] = f / aspect;
    p.m[5] = f;
    p.m[10] = (far_z + near_z) * nf;
    p.m[11] = -1.0;
    p.m[14] = 2.0 * far_z * near_z * nf;
    return p;
}

fn lookAt(
    eye_x: f32, eye_y: f32, eye_z: f32,
    at_x: f32, at_y: f32, at_z: f32,
    up_x: f32, up_y: f32, up_z: f32,
) Mat4 {
    var fx = at_x - eye_x;
    var fy = at_y - eye_y;
    var fz = at_z - eye_z;

    var len = std.math.sqrt(f32, fx * fx + fy * fy + fz * fz);
    if (len > 0.0) {
        fx /= len;
        fy /= len;
        fz /= len;
    }

    var ux = up_x;
    var uy = up_y;
    var uz = up_z;

    var sx = uy * fz - uz * fy;
    var sy = uz * fx - ux * fz;
    var sz = ux * fy - uy * fx;

    len = std.math.sqrt(f32, sx * sx + sy * sy + sz * sz);
    if (len > 0.0) {
        sx /= len;
        sy /= len;
        sz /= len;
    }

    ux = fy * sz - fz * sy;
    uy = fz * sx - fx * sz;
    uz = fx * sy - fy * sx;

    var v = Mat4{ .m = .{0} ** 16 };
    v.m[0] = sx;
    v.m[4] = sy;
    v.m[8] = sz;
    v.m[12] = -(sx * eye_x + sy * eye_y + sz * eye_z);

    v.m[1] = ux;
    v.m[5] = uy;
    v.m[9] = uz;
    v.m[13] = -(ux * eye_x + uy * eye_y + uz * eye_z);

    v.m[2] = -fx;
    v.m[6] = -fy;
    v.m[10] = -fz;
    v.m[14] = (fx * eye_x + fy * eye_y + fz * eye_z);

    v.m[3] = 0.0;
    v.m[7] = 0.0;
    v.m[11] = 0.0;
    v.m[15] = 1.0;
    return v;
}

fn mat4Mul(out: *Mat4, a: *const Mat4, b: *const Mat4) void {
    var col: usize = 0;
    while (col < 4) : (col += 1) {
        var row: usize = 0;
        while (row < 4) : (row += 1) {
            out.m[col * 4 + row] =
                a.m[0 * 4 + row] * b.m[col * 4 + 0] +
                a.m[1 * 4 + row] * b.m[col * 4 + 1] +
                a.m[2 * 4 + row] * b.m[col * 4 + 2] +
                a.m[3 * 4 + row] * b.m[col * 4 + 3];
        }
    }
}

fn loadFile(allocator: std.mem.Allocator, path: []const u8) ![]u8 {
    var file = try std.fs.cwd().openFile(path, .{ .mode = .read_only });
    defer file.close();

    const stat = try file.stat();
    const size_usize: usize = @intCast(stat.size);

    var buf = try allocator.alloc(u8, size_usize);
    const read_n = try file.readAll(buf);
    return buf[0..read_n];
}

// ---------- Simple ECS in Zig ----------

const Position = struct {
    x: f32,
    y: f32,
    z: f32,
};

const Entity = struct {
    id: i32,
    position: ?Position = null,
};

const World = struct {
    const Self = @This();

    entities: std.ArrayList(Entity),
    next_entity_id: i32 = 0,

    fn init(allocator: std.mem.Allocator) Self {
        return .{
            .entities = std.ArrayList(Entity).init(allocator),
            .next_entity_id = 0,
        };
    }

    fn deinit(self: *Self) void {
        self.entities.deinit();
    }

    fn createEntity(self: *Self) !*Entity {
        const id = self.next_entity_id;
        self.next_entity_id += 1;
        try self.entities.append(.{ .id = id });
        return &self.entities.items[self.entities.items.len - 1];
    }

    fn queryWithPosition(self: *Self) []Entity {
        // For this simple demo, we just return a slice of all entities;
        // callers filter on `position != null`.
        return self.entities.items;
    }
};

// ---------- Zig demo main ----------

pub fn main() !void {
    std.debug.print("Starting PlushEngine Zig demo (math + ECS)\n", .{});

    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    // Math demo (same logic as C++)
    const view = lookAt(1.2, 1.0, 1.8, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);
    const aspect: f32 = 1280.0 / 720.0;
    const proj = perspective(60.0, aspect, 0.1, 100.0);

    var mvp = Mat4{ .m = .{0} ** 16 };
    mat4Mul(&mvp, &proj, &view);
    std.debug.print("MVP[0] = {d:.4}\n", .{mvp.m[0]});

    const vert_spv = loadFile(allocator, "shaders/cube.vert.spv") catch |err| {
        std.debug.print("Could not load shaders/cube.vert.spv: {s}\n", .{@errorName(err)});
        return;
    };
    defer allocator.free(vert_spv);
    std.debug.print("Loaded {d} bytes of vertex shader SPIR-V in Zig.\n", .{vert_spv.len});

    // ECS demo: create entity, add component, and query.
    var world = World.init(allocator);
    defer world.deinit();

    var e = try world.createEntity();
    e.position = Position{ .x = 1.0, .y = 2.0, .z = 3.0 };

    std.debug.print("Created entity id={d} with Position component.\n", .{e.id});

    const all_entities = world.queryWithPosition();
    var count_with_pos: usize = 0;
    for (all_entities) |ent| {
        if (ent.position) |p| {
            count_with_pos += 1;
            std.debug.print(
                "Entity {d} has Position({d:.2}, {d:.2}, {d:.2})\n",
                .{ ent.id, p.x, p.y, p.z },
            );
        }
    }
    std.debug.print("Query found {d} entities with Position.\n", .{count_with_pos});
}

