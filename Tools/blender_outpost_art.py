"""Build the OUTPOST Design V3 procedural sprite atlas in Blender.

Run with Blender (4.2+ recommended)::

    blender --background --python Tools/blender_outpost_art.py

Optional arguments after ``--``::

    --output-dir SourceArt/DesignV3 --atlas-size 2048 --blend-name outpost_design_v3.blend

The script creates one high-angle orthographic 8x8 render. Rows 0-6 are eight facing
directions (column 0 faces world +X; columns advance clockwise by 45 degrees).
Row 7 holds individual props.  Every cell has a centered ground anchor and
transparent padding.  Geometry is intentionally low-poly/chunky so the source
blend stays compact and the silhouettes remain readable at gameplay scale.
"""

from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector


GRID = 8
CELL_WORLD = 4.0
DEFAULT_ATLAS_SIZE = 2048
CAMERA_ELEVATION_DEGREES = 68.0


PALETTE = {
    "navy": (0.025, 0.045, 0.060, 1.0),
    "steel": (0.105, 0.145, 0.165, 1.0),
    "steel_light": (0.150, 0.205, 0.220, 1.0),
    "edge": (0.265, 0.335, 0.345, 1.0),
    "teal": (0.035, 0.290, 0.300, 1.0),
    "cyan": (0.010, 0.780, 0.900, 1.0),
    "cyan_white": (0.380, 0.950, 1.000, 1.0),
    "amber": (0.950, 0.255, 0.018, 1.0),
    "amber_hot": (1.000, 0.710, 0.100, 1.0),
    "crimson": (0.510, 0.025, 0.055, 1.0),
    "red_hot": (1.000, 0.060, 0.025, 1.0),
    "violet": (0.280, 0.055, 0.430, 1.0),
    "violet_hot": (0.850, 0.110, 1.000, 1.0),
    "bone": (0.540, 0.430, 0.280, 1.0),
    "rubber": (0.020, 0.025, 0.030, 1.0),
}


def cli_args() -> argparse.Namespace:
    argv = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", default="SourceArt/DesignV3")
    parser.add_argument("--atlas-size", type=int, default=DEFAULT_ATLAS_SIZE)
    parser.add_argument("--blend-name", default="outpost_design_v3.blend")
    parser.add_argument("--atlas-name", default="sprites_atlas.png")
    parser.add_argument("--metadata-name", default="sprites_atlas.json")
    return parser.parse_args(argv)


def resolve_output_dir(value: str) -> Path:
    target = Path(value)
    if target.is_absolute():
        return target.resolve()
    # This file lives at <project>/Tools/blender_outpost_art.py.
    return (Path(__file__).resolve().parents[1] / target).resolve()


def clear_scene() -> None:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for collection in list(bpy.data.collections):
        if collection.name != "Collection":
            bpy.data.collections.remove(collection)
    for datablocks in (bpy.data.meshes, bpy.data.curves, bpy.data.cameras, bpy.data.lights):
        for block in list(datablocks):
            if block.users == 0:
                datablocks.remove(block)


def material(
    name: str,
    color: tuple[float, float, float, float],
    metallic: float = 0.0,
    roughness: float = 0.55,
    emission: tuple[float, float, float, float] | None = None,
    emission_strength: float = 0.0,
) -> bpy.types.Material:
    existing = bpy.data.materials.get(name)
    if existing:
        return existing
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = color
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = color
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    if emission is not None:
        emission_input = bsdf.inputs.get("Emission Color") or bsdf.inputs.get("Emission")
        if emission_input:
            emission_input.default_value = emission
        strength_input = bsdf.inputs.get("Emission Strength")
        if strength_input:
            strength_input.default_value = emission_strength
    return mat


def build_materials() -> dict[str, bpy.types.Material]:
    mats = {
        "navy": material("M_Navy", PALETTE["navy"], 0.25, 0.38),
        "steel": material("M_Steel", PALETTE["steel"], 0.72, 0.30),
        "steel_light": material("M_SteelLight", PALETTE["steel_light"], 0.75, 0.25),
        "edge": material("M_Edge", PALETTE["edge"], 0.65, 0.23),
        "teal": material("M_TealArmor", PALETTE["teal"], 0.58, 0.28),
        "crimson": material("M_CrimsonChitin", PALETTE["crimson"], 0.35, 0.32),
        "violet": material("M_VioletArmor", PALETTE["violet"], 0.52, 0.27),
        "amber_paint": material("M_AmberPaint", (0.78, 0.145, 0.012, 1.0), 0.28, 0.40),
        "crystal": material("M_CyanCrystal", PALETTE["cyan"], 0.08, 0.20, PALETTE["cyan"], 1.45),
        "bone": material("M_BoneClaw", PALETTE["bone"], 0.05, 0.62),
        "rubber": material("M_Rubber", PALETTE["rubber"], 0.0, 0.73),
    }
    for key, color, strength in (
        ("cyan", PALETTE["cyan"], 2.0),
        ("cyan_white", PALETTE["cyan_white"], 2.6),
        ("amber", PALETTE["amber"], 1.8),
        ("amber_hot", PALETTE["amber_hot"], 3.4),
        ("red_hot", PALETTE["red_hot"], 3.0),
        ("violet_hot", PALETTE["violet_hot"], 2.5),
    ):
        mats[key] = material(f"M_{key.title()}", color, 0.15, 0.26, color, strength)
    return mats


def link_only(obj: bpy.types.Object, collection: bpy.types.Collection) -> bpy.types.Object:
    for owner in list(obj.users_collection):
        owner.objects.unlink(obj)
    collection.objects.link(obj)
    return obj


def finish_mesh(
    obj: bpy.types.Object,
    collection: bpy.types.Collection,
    mat: bpy.types.Material,
    bevel: float = 0.06,
) -> bpy.types.Object:
    obj.data.materials.append(mat)
    if bevel > 0.0:
        modifier = obj.modifiers.new("Edge bevel", "BEVEL")
        modifier.width = bevel
        modifier.segments = 2
        modifier.limit_method = "ANGLE"
    return link_only(obj, collection)


def box(
    collection: bpy.types.Collection,
    name: str,
    loc: tuple[float, float, float],
    scale: tuple[float, float, float],
    mat: bpy.types.Material,
    bevel: float = 0.06,
    rotation: tuple[float, float, float] = (0.0, 0.0, 0.0),
) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cube_add(location=loc, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.scale = (scale[0] * 0.5, scale[1] * 0.5, scale[2] * 0.5)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return finish_mesh(obj, collection, mat, bevel)


def cyl(
    collection: bpy.types.Collection,
    name: str,
    loc: tuple[float, float, float],
    radius: float,
    depth: float,
    mat: bpy.types.Material,
    vertices: int = 12,
    rotation: tuple[float, float, float] = (0.0, 0.0, 0.0),
    scale: tuple[float, float, float] = (1.0, 1.0, 1.0),
    bevel: float = 0.035,
) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=loc, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return finish_mesh(obj, collection, mat, bevel)


def sphere(
    collection: bpy.types.Collection,
    name: str,
    loc: tuple[float, float, float],
    scale: tuple[float, float, float],
    mat: bpy.types.Material,
    segments: int = 16,
    rings: int = 8,
) -> bpy.types.Object:
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments, ring_count=rings, location=loc)
    obj = bpy.context.object
    obj.name = name
    obj.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return finish_mesh(obj, collection, mat, 0.025)


def cone(
    collection: bpy.types.Collection,
    name: str,
    loc: tuple[float, float, float],
    radius1: float,
    radius2: float,
    depth: float,
    mat: bpy.types.Material,
    vertices: int = 6,
    rotation: tuple[float, float, float] = (0.0, 0.0, 0.0),
) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cone_add(
        vertices=vertices, radius1=radius1, radius2=radius2, depth=depth, location=loc, rotation=rotation
    )
    obj = bpy.context.object
    obj.name = name
    return finish_mesh(obj, collection, mat, 0.025)


def bar_between(
    collection: bpy.types.Collection,
    name: str,
    start: tuple[float, float, float],
    end: tuple[float, float, float],
    radius: float,
    mat: bpy.types.Material,
    vertices: int = 10,
) -> bpy.types.Object:
    a, b = Vector(start), Vector(end)
    delta = b - a
    midpoint = (a + b) * 0.5
    obj = cyl(collection, name, midpoint, radius, delta.length, mat, vertices=vertices, bevel=radius * 0.2)
    obj.rotation_mode = "QUATERNION"
    obj.rotation_quaternion = Vector((0.0, 0.0, 1.0)).rotation_difference(delta.normalized())
    return obj


def new_asset_collection(name: str) -> bpy.types.Collection:
    return bpy.data.collections.new(f"SRC_{name}")


def add_boot_pair(c: bpy.types.Collection, mats: dict, size: float = 1.0) -> None:
    for side in (-1.0, 1.0):
        box(c, "Boot", (-0.23 * size, side * 0.23 * size, 0.16 * size), (0.56, 0.30, 0.28), mats["rubber"], 0.07)
        box(c, "Boot cap", (0.01 * size, side * 0.23 * size, 0.20 * size), (0.18, 0.31, 0.14), mats["steel_light"], 0.035)


def build_player(kind: str, mats: dict) -> bpy.types.Collection:
    c = new_asset_collection(f"player_{kind}")
    add_boot_pair(c, mats)
    box(c, "Torso", (-0.04, 0.0, 0.69), (0.86, 0.72, 0.66), mats["teal"], 0.12)
    box(c, "Chest plate", (0.30, 0.0, 0.78), (0.16, 0.58, 0.38), mats["steel_light"], 0.055)
    box(c, "Backpack", (-0.48, 0.0, 0.73), (0.28, 0.60, 0.58), mats["steel"], 0.07)
    box(c, "Pack light", (-0.64, 0.0, 0.75), (0.05, 0.30, 0.14), mats["cyan"], 0.012)
    for side in (-1.0, 1.0):
        sphere(c, "Shoulder", (0.02, side * 0.46, 0.79), (0.29, 0.23, 0.25), mats["steel_light"])
    sphere(c, "Helmet", (0.12, 0.0, 1.20), (0.48, 0.43, 0.40), mats["teal"])
    box(c, "Helmet crown", (0.01, 0.0, 1.53), (0.42, 0.44, 0.12), mats["steel_light"], 0.05)
    box(c, "Visor", (0.48, 0.0, 1.24), (0.13, 0.58, 0.20), mats["cyan_white"], 0.04)
    box(c, "Visor divider", (0.55, 0.0, 1.24), (0.04, 0.045, 0.22), mats["navy"], 0.01)

    if kind == "gun":
        bar_between(c, "Left forearm", (0.18, 0.33, 0.78), (0.54, 0.16, 0.71), 0.11, mats["teal"])
        bar_between(c, "Right forearm", (0.16, -0.38, 0.77), (0.57, -0.26, 0.70), 0.11, mats["teal"])
        box(c, "Rifle body", (0.64, -0.20, 0.70), (0.74, 0.24, 0.24), mats["steel"], 0.045)
        box(c, "Rifle receiver", (0.72, -0.20, 0.84), (0.30, 0.20, 0.15), mats["steel_light"], 0.035)
        cyl(c, "Rifle barrel", (1.18, -0.20, 0.71), 0.07, 0.66, mats["rubber"], rotation=(0.0, math.pi / 2.0, 0.0))
        cyl(c, "Muzzle glow", (1.52, -0.20, 0.71), 0.095, 0.09, mats["amber_hot"], rotation=(0.0, math.pi / 2.0, 0.0))
        box(c, "Gun cell", (0.56, -0.34, 0.73), (0.19, 0.09, 0.18), mats["cyan"], 0.02)
    elif kind == "pickaxe":
        bar_between(c, "Left arm", (0.08, 0.42, 0.78), (0.58, 0.22, 0.90), 0.115, mats["teal"])
        bar_between(c, "Right arm", (0.08, -0.42, 0.78), (0.52, -0.10, 0.92), 0.115, mats["teal"])
        bar_between(c, "Pick handle", (0.36, -0.12, 0.58), (1.20, 0.25, 1.20), 0.065, mats["bone"])
        bar_between(c, "Pick head", (0.93, -0.15, 1.20), (1.45, 0.55, 1.20), 0.085, mats["steel_light"])
        cone(c, "Pick point", (1.50, 0.62, 1.20), 0.13, 0.0, 0.32, mats["edge"], vertices=6, rotation=(math.pi / 2.0, 0.0, -0.64))
        box(c, "Tool glow", (1.11, 0.09, 1.23), (0.18, 0.10, 0.12), mats["cyan"], 0.02)
    else:
        for side in (-1.0, 1.0):
            bar_between(c, "Arm", (0.05, side * 0.43, 0.79), (0.55, side * 0.47, 0.64), 0.13, mats["teal"])
            sphere(c, "Glove", (0.69, side * 0.48, 0.61), (0.18, 0.16, 0.16), mats["steel_light"], 12, 6)
    return c


def build_grunt(mats: dict) -> bpy.types.Collection:
    c = new_asset_collection("grunt")
    sphere(c, "Chitin abdomen", (-0.28, 0.0, 0.61), (0.71, 0.54, 0.43), mats["crimson"], 14, 7)
    sphere(c, "Head", (0.47, 0.0, 0.48), (0.45, 0.42, 0.34), mats["crimson"], 12, 6)
    box(c, "Back plate", (-0.28, 0.0, 0.91), (0.67, 0.68, 0.16), mats["steel"], 0.07)
    for side in (-1.0, 1.0):
        for x, spread in ((0.36, 0.86), (-0.12, 1.02), (-0.52, 0.82)):
            knee = (x + 0.05, side * spread, 0.34)
            foot = (x + (0.28 if x > 0 else -0.18), side * (spread + 0.32), 0.10)
            bar_between(c, "Leg upper", (x, side * 0.38, 0.48), knee, 0.085, mats["crimson"], 8)
            bar_between(c, "Leg claw", knee, foot, 0.060, mats["bone"], 7)
    for side in (-1.0, 1.0):
        sphere(c, "Eye", (0.82, side * 0.18, 0.58), (0.11, 0.08, 0.08), mats["red_hot"], 10, 5)
    cone(c, "Snout", (0.89, 0.0, 0.42), 0.28, 0.04, 0.50, mats["crimson"], vertices=6, rotation=(0.0, math.pi / 2.0, 0.0))
    return c


def build_runner(mats: dict) -> bpy.types.Collection:
    c = new_asset_collection("runner")
    sphere(c, "Runner body", (-0.10, 0.0, 0.50), (0.70, 0.34, 0.30), mats["amber_paint"], 12, 6)
    cone(c, "Runner head", (0.63, 0.0, 0.50), 0.33, 0.04, 0.66, mats["amber_paint"], vertices=6, rotation=(0.0, math.pi / 2.0, 0.0))
    box(c, "Spine", (-0.16, 0.0, 0.79), (0.78, 0.25, 0.13), mats["steel"], 0.04)
    for side in (-1.0, 1.0):
        for x in (0.22, -0.39):
            knee = (x - 0.10, side * 0.61, 0.30)
            foot = (x + 0.26, side * 1.03, 0.09)
            bar_between(c, "Runner thigh", (x, side * 0.22, 0.43), knee, 0.072, mats["amber_paint"], 7)
            bar_between(c, "Runner shin", knee, foot, 0.048, mats["bone"], 7)
    for side in (-1.0, 1.0):
        sphere(c, "Runner eye", (0.79, side * 0.12, 0.58), (0.075, 0.055, 0.055), mats["amber_hot"], 8, 4)
    bar_between(c, "Tail", (-0.59, 0.0, 0.48), (-1.03, 0.0, 0.22), 0.065, mats["bone"], 7)
    return c


def build_brute(mats: dict) -> bpy.types.Collection:
    c = new_asset_collection("brute")
    sphere(c, "Brute core", (-0.15, 0.0, 0.73), (0.88, 0.72, 0.58), mats["violet"], 14, 7)
    box(c, "Dorsal armor", (-0.24, 0.0, 1.18), (1.12, 0.96, 0.26), mats["steel"], 0.10)
    box(c, "Armor stripe", (-0.17, 0.0, 1.34), (0.65, 0.19, 0.08), mats["violet_hot"], 0.025)
    cone(c, "Brute face", (0.74, 0.0, 0.66), 0.48, 0.15, 0.72, mats["violet"], vertices=7, rotation=(0.0, math.pi / 2.0, 0.0))
    for side in (-1.0, 1.0):
        shoulder = (0.02, side * 0.72, 0.75)
        fist = (0.48, side * 1.17, 0.28)
        sphere(c, "Shoulder armor", shoulder, (0.43, 0.39, 0.36), mats["steel_light"], 12, 6)
        bar_between(c, "Heavy arm", shoulder, fist, 0.20, mats["violet"], 10)
        sphere(c, "Heavy claw", fist, (0.36, 0.28, 0.22), mats["bone"], 10, 5)
        box(c, "Eye", (0.92, side * 0.17, 0.75), (0.10, 0.12, 0.13), mats["violet_hot"], 0.025)
    return c


def build_boss(mats: dict) -> bpy.types.Collection:
    c = new_asset_collection("boss")
    sphere(c, "Boss body", (-0.22, 0.0, 0.76), (1.04, 0.83, 0.65), mats["crimson"], 16, 8)
    for x, width, height in ((-0.63, 1.12, 0.24), (-0.13, 1.31, 0.28), (0.38, 1.05, 0.23)):
        box(c, "Boss plate", (x, 0.0, 1.24 + 0.08 * x), (0.47, width, height), mats["steel"], 0.10)
        box(c, "Plate ember", (x + 0.08, 0.0, 1.40 + 0.08 * x), (0.20, width * 0.48, 0.06), mats["red_hot"], 0.018)
    cone(c, "Boss helm", (0.88, 0.0, 0.72), 0.57, 0.12, 0.82, mats["crimson"], vertices=7, rotation=(0.0, math.pi / 2.0, 0.0))
    for side in (-1.0, 1.0):
        hip = (-0.18, side * 0.62, 0.67)
        knee = (0.08, side * 1.18, 0.38)
        foot = (0.55, side * 1.42, 0.11)
        sphere(c, "Boss shoulder", hip, (0.48, 0.44, 0.40), mats["steel_light"], 12, 6)
        bar_between(c, "Boss limb", hip, knee, 0.22, mats["crimson"], 10)
        bar_between(c, "Boss talon", knee, foot, 0.14, mats["bone"], 8)
        box(c, "Boss eye", (1.13, side * 0.20, 0.82), (0.13, 0.16, 0.15), mats["red_hot"], 0.025)
        cone(c, "Shoulder spike", (-0.35, side * 0.87, 1.23), 0.17, 0.0, 0.55, mats["crimson"], vertices=6)
    return c


def hazard_stripes(c: bpy.types.Collection, mats: dict, y: float, z: float, width: float) -> None:
    for x in (-0.78, -0.26, 0.26, 0.78):
        box(c, "Hazard stripe", (x * width, y, z), (0.25, 0.07, 0.08), mats["amber"], 0.01, (0.0, 0.0, -0.42))


def build_forge(mats: dict) -> bpy.types.Collection:
    c = new_asset_collection("forge")
    box(c, "Forge base", (0.0, 0.0, 0.30), (2.65, 2.18, 0.58), mats["steel"], 0.11)
    box(c, "Forge body", (-0.18, 0.0, 0.88), (1.92, 1.75, 0.72), mats["steel_light"], 0.12)
    box(c, "Furnace mouth", (0.82, 0.0, 0.91), (0.16, 1.03, 0.43), mats["amber_hot"], 0.04)
    for y in (-0.39, -0.13, 0.13, 0.39):
        box(c, "Furnace grille", (0.92, y, 0.92), (0.08, 0.06, 0.51), mats["rubber"], 0.01)
    cyl(c, "Forge chimney", (-0.60, 0.56, 1.47), 0.25, 0.88, mats["steel"], 12)
    cyl(c, "Chimney rim", (-0.60, 0.56, 1.93), 0.31, 0.14, mats["edge"], 12)
    box(c, "Anvil", (-0.18, -0.38, 1.41), (0.72, 0.38, 0.19), mats["edge"], 0.05)
    # A top-facing crucible opening remains legible from the 68-degree camera.
    cyl(c, "Furnace top rim", (0.27, -0.30, 1.30), 0.48, 0.14, mats["rubber"], 12)
    cyl(c, "Furnace top glow", (0.27, -0.30, 1.39), 0.36, 0.08, mats["amber_hot"], 12, bevel=0.02)
    cyl(c, "Furnace inner heat", (0.27, -0.30, 1.45), 0.19, 0.05, mats["amber"], 10, bevel=0.015)
    box(c, "Status", (0.40, 0.70, 1.27), (0.32, 0.10, 0.16), mats["cyan"], 0.025)
    hazard_stripes(c, mats, -1.10, 0.63, 0.92)
    return c


def build_ore(mats: dict) -> bpy.types.Collection:
    c = new_asset_collection("ore")
    sphere(c, "Ore rock", (0.0, 0.0, 0.22), (1.10, 0.84, 0.28), mats["navy"], 12, 6)
    crystals = [
        ((0.00, 0.00, 0.91), 0.33, 1.55),
        ((-0.48, 0.17, 0.67), 0.25, 1.10),
        ((0.43, 0.27, 0.62), 0.23, 1.03),
        ((0.20, -0.42, 0.53), 0.21, 0.87),
        ((-0.48, -0.40, 0.43), 0.17, 0.67),
    ]
    for i, (loc, radius, depth) in enumerate(crystals):
        cone(c, f"Crystal {i}", loc, radius, 0.02, depth, mats["crystal"], vertices=6)
        box(c, f"Crystal core {i}", (loc[0] + 0.03, loc[1] - 0.02, loc[2] - depth * 0.12), (0.07, 0.07, depth * 0.42), mats["cyan"], 0.01)
    return c


def build_wall(mats: dict, vertical: bool) -> bpy.types.Collection:
    c = new_asset_collection("wall_v" if vertical else "wall_h")
    length = 2.82
    dims = (1.02, length, 0.70) if vertical else (length, 1.02, 0.70)
    box(c, "Barricade body", (0.0, 0.0, 0.42), dims, mats["steel"], 0.10)
    rail_dims = (0.16, length * 0.82, 0.25) if vertical else (length * 0.82, 0.16, 0.25)
    box(c, "Raised rail", (0.0, 0.0, 0.86), rail_dims, mats["steel_light"], 0.05)
    positions = ((0.0, -1.14), (0.0, 1.14)) if vertical else ((-1.14, 0.0), (1.14, 0.0))
    for x, y in positions:
        box(c, "Wall cap", (x, y, 0.63), (0.90, 0.37, 0.94) if vertical else (0.37, 0.90, 0.94), mats["edge"], 0.07)
        box(c, "Wall lamp", (x, y, 1.12), (0.35, 0.18, 0.08), mats["amber_hot"], 0.02)
    stripe_positions = (-0.55, 0.0, 0.55)
    for offset in stripe_positions:
        loc = (0.52, offset, 0.58) if vertical else (offset, -0.52, 0.58)
        dims2 = (0.06, 0.28, 0.18) if vertical else (0.28, 0.06, 0.18)
        box(c, "Wall hazard", loc, dims2, mats["amber"], 0.01)
    return c


def build_generator(mats: dict) -> bpy.types.Collection:
    c = new_asset_collection("generator")
    cyl(c, "Generator base", (0.0, 0.0, 0.24), 1.18, 0.46, mats["steel"], 12)
    cyl(c, "Generator ring", (0.0, 0.0, 0.59), 0.95, 0.30, mats["steel_light"], 12)
    cyl(c, "Energy core", (0.0, 0.0, 1.05), 0.45, 1.18, mats["cyan_white"], 12)
    cyl(c, "Core crown", (0.0, 0.0, 1.68), 0.58, 0.18, mats["edge"], 12)
    for angle in range(0, 360, 90):
        a = math.radians(angle)
        x, y = math.cos(a) * 0.95, math.sin(a) * 0.95
        box(c, "Generator brace", (x, y, 0.78), (0.42, 0.42, 0.95), mats["steel"], 0.07, (0.0, 0.0, a))
        box(c, "Brace light", (x * 1.04, y * 1.04, 0.84), (0.18, 0.11, 0.24), mats["amber"], 0.02, (0.0, 0.0, a))
    return c


def build_turret(mats: dict) -> bpy.types.Collection:
    c = new_asset_collection("turret")
    box(c, "Turret base", (0.0, 0.0, 0.27), (2.05, 1.82, 0.54), mats["steel"], 0.11)
    cyl(c, "Turntable", (0.0, 0.0, 0.66), 0.77, 0.38, mats["steel_light"], 12)
    box(c, "Turret housing", (0.18, 0.0, 1.08), (1.16, 1.00, 0.69), mats["steel"], 0.10)
    for side in (-1.0, 1.0):
        cyl(c, "Cannon", (1.07, side * 0.27, 1.18), 0.105, 1.55, mats["edge"], 12, rotation=(0.0, math.pi / 2.0, 0.0))
        cyl(c, "Muzzle", (1.85, side * 0.27, 1.18), 0.15, 0.16, mats["rubber"], 12, rotation=(0.0, math.pi / 2.0, 0.0))
    box(c, "Turret sight", (0.63, 0.0, 1.52), (0.26, 0.30, 0.20), mats["cyan"], 0.04)
    box(c, "Turret amber", (-0.47, 0.0, 1.10), (0.10, 0.48, 0.22), mats["amber"], 0.03)
    return c


def build_supply(mats: dict) -> bpy.types.Collection:
    c = new_asset_collection("supply")
    box(c, "Supply crate", (0.0, 0.0, 0.52), (2.28, 1.72, 1.02), mats["steel"], 0.12)
    for x in (-0.86, 0.86):
        box(c, "Crate rib", (x, 0.0, 0.61), (0.20, 1.82, 1.14), mats["edge"], 0.04)
    box(c, "Crate inset", (0.0, -0.88, 0.58), (0.92, 0.07, 0.35), mats["amber"], 0.025)
    box(c, "Crate status", (0.0, 0.88, 0.59), (0.46, 0.07, 0.18), mats["cyan"], 0.02)
    return c


def build_terminal(mats: dict) -> bpy.types.Collection:
    c = new_asset_collection("terminal")
    box(c, "Terminal plinth", (0.0, 0.0, 0.25), (1.70, 1.48, 0.48), mats["steel"], 0.09)
    box(c, "Terminal stem", (-0.18, 0.0, 0.85), (0.82, 0.98, 1.15), mats["steel_light"], 0.08)
    box(c, "Terminal face", (0.32, 0.0, 1.19), (0.18, 0.75, 0.54), mats["cyan"], 0.04, (0.0, -0.16, 0.0))
    box(c, "Terminal hood", (0.24, 0.0, 1.55), (0.52, 0.97, 0.18), mats["edge"], 0.05)
    for y in (-0.30, 0.30):
        box(c, "Terminal lamp", (-0.47, y, 0.93), (0.12, 0.18, 0.20), mats["amber"], 0.025)
    return c


def instance_collection(
    source: bpy.types.Collection,
    name: str,
    col: int,
    row: int,
    rotation_z: float = 0.0,
    scale: float = 1.0,
) -> bpy.types.Object:
    x = (col - (GRID - 1) * 0.5) * CELL_WORLD
    # At a camera elevation E, ground-plane Y projects to screen Y * sin(E).
    # Expanding the source-grid spacing by 1/sin(E) preserves exact square
    # atlas cells while retaining enough side faces to read the bevels.
    projected_y = ((GRID - 1) * 0.5 - row) * CELL_WORLD
    y = projected_y / math.sin(math.radians(CAMERA_ELEVATION_DEGREES))
    obj = bpy.data.objects.new(name, None)
    obj.instance_type = "COLLECTION"
    obj.instance_collection = source
    obj.location = (x, y, 0.0)
    obj.rotation_euler[2] = rotation_z
    obj.scale = (scale, scale, scale)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def setup_scene(atlas_size: int, output_png: Path) -> None:
    scene = bpy.context.scene
    # Generated source files are deterministic; do not leave .blend1 revisions.
    bpy.context.preferences.filepaths.save_version = 0
    scene.render.engine = "BLENDER_EEVEE_NEXT"
    scene.render.resolution_x = atlas_size
    scene.render.resolution_y = atlas_size
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.image_settings.color_depth = "8"
    scene.render.film_transparent = True
    scene.render.filepath = str(output_png)
    scene.render.image_settings.color_mode = "RGBA"
    scene.view_settings.look = "AgX - Medium High Contrast"
    scene.render.resolution_percentage = 100
    scene.render.engine = "BLENDER_EEVEE_NEXT"

    camera_data = bpy.data.cameras.new("AtlasCamera")
    camera = bpy.data.objects.new("AtlasCamera", camera_data)
    bpy.context.scene.collection.objects.link(camera)
    elevation = math.radians(CAMERA_ELEVATION_DEGREES)
    distance = 40.0
    camera.location = (0.0, -math.cos(elevation) * distance, math.sin(elevation) * distance)
    camera.rotation_euler = (Vector((0.0, 0.0, 0.0)) - camera.location).to_track_quat("-Z", "Y").to_euler()
    camera_data.type = "ORTHO"
    camera_data.ortho_scale = GRID * CELL_WORLD
    camera_data.lens = 50.0
    scene.camera = camera

    world = bpy.data.worlds.new("OutpostWorld") if not bpy.data.worlds else bpy.data.worlds[0]
    world.use_nodes = True
    bg = world.node_tree.nodes.get("Background")
    bg.inputs["Color"].default_value = (0.018, 0.028, 0.038, 1.0)
    bg.inputs["Strength"].default_value = 0.28
    scene.world = world

    sun_data = bpy.data.lights.new("UpperLeftKey", "SUN")
    sun_data.energy = 2.35
    sun_data.angle = math.radians(18.0)
    sun = bpy.data.objects.new("UpperLeftKey", sun_data)
    sun.rotation_euler = (math.radians(24.0), math.radians(-32.0), math.radians(-138.0))
    scene.collection.objects.link(sun)

    fill_data = bpy.data.lights.new("CoolFill", "AREA")
    fill_data.energy = 430.0
    fill_data.color = (0.17, 0.42, 0.50)
    fill_data.shape = "DISK"
    fill_data.size = 28.0
    fill = bpy.data.objects.new("CoolFill", fill_data)
    fill.location = (-9.0, 10.0, 24.0)
    scene.collection.objects.link(fill)

    for prop_name in ("bloom", "gtao"):
        # Blender releases expose Eevee settings differently. The material,
        # bevel and directional lighting remain valid when these are absent.
        try:
            setattr(scene, f"use_{prop_name}", True)
        except Exception:
            pass


def cell_record(name: str, row: int, col: int, atlas_size: int, facing: str | None = None) -> dict:
    cell_px = atlas_size // GRID
    record = {
        "name": name,
        "row": row,
        "column": col,
        "pixel_rect_top_left": {"x": col * cell_px, "y": row * cell_px, "width": cell_px, "height": cell_px},
        "uv_rect_top_left_origin": {
            "u": col / GRID,
            "v": row / GRID,
            "width": 1.0 / GRID,
            "height": 1.0 / GRID,
        },
        "anchor_normalized": {"x": 0.5, "y": 0.5},
        "cell_world_extent": {"x": CELL_WORLD, "y": CELL_WORLD},
    }
    if facing is not None:
        record["facing"] = facing
    return record


def write_metadata(path: Path, atlas_name: str, blend_name: str, atlas_size: int, cells: list[dict]) -> None:
    cell_px = atlas_size // GRID
    data = {
        "format_version": 1,
        "generator": "Tools/blender_outpost_art.py",
        "source_blend": blend_name,
        "atlas": atlas_name,
        "atlas_size_px": {"width": atlas_size, "height": atlas_size},
        "grid": {"columns": GRID, "rows": GRID, "cell_width_px": cell_px, "cell_height_px": cell_px},
        "camera": {
            "projection": "orthographic_high_angle",
            "elevation_degrees": CAMERA_ELEVATION_DEGREES,
            "view_azimuth": "from world -Y toward +Y",
            "world_x_screen_axis": "right",
            "world_y_ground_projection_screen_axis": "up",
            "orthographic_extent_world": GRID * CELL_WORLD,
            "pixels_per_world_unit": atlas_size / (GRID * CELL_WORLD),
            "ground_y_source_spacing_per_row": CELL_WORLD / math.sin(math.radians(CAMERA_ELEVATION_DEGREES)),
        },
        "sampling": {
            "texture_address": "clamp",
            "recommended_filter": "nearest_or_bilinear_without_mips",
            "transparent_padding_px": 24,
            "cell_inset_uv": 0.5 / atlas_size,
        },
        "orientation": {
            "frame_0": "world +X / screen right",
            "sequence": "screen-clockwise",
            "degrees_about_world_z": [0, -45, -90, -135, -180, -225, -270, -315],
        },
        "anchor": {
            "normalized": {"x": 0.5, "y": 0.5},
            "meaning": "ground center; rotate, scale, and bob about this point",
        },
        "runtime_scale_recommendation": {
            "actor_nominal_cell_world_width": 1.0,
            "atlas_source_world_units_per_cell": CELL_WORLD,
            "suggested_player_screen_diameter_px": 58,
            "suggested_grunt_screen_diameter_px": 62,
            "suggested_runner_screen_diameter_px": 52,
            "suggested_brute_screen_diameter_px": 78,
            "suggested_boss_screen_diameter_px": 112,
            "note": "Scale the full cell quad; transparent padding preserves a stable pivot and prevents frame jitter.",
        },
        "future_design_only": ["generator", "turret"],
        "cells": cells,
    }
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def measure_alpha_bounds(atlas_path: Path, cells: list[dict], atlas_size: int) -> None:
    """Record true rendered bounds and fail if a cell can bleed into a neighbor."""
    import numpy as np

    image = bpy.data.images.load(str(atlas_path), check_existing=False)
    width, height = image.size
    if width != atlas_size or height != atlas_size:
        raise RuntimeError(f"Rendered atlas is {width}x{height}, expected {atlas_size}x{atlas_size}")
    rgba = np.empty(width * height * 4, dtype=np.float32)
    image.pixels.foreach_get(rgba)
    alpha = rgba.reshape(height, width, 4)[:, :, 3]
    cell_px = atlas_size // GRID
    for record in cells:
        row, col = record["row"], record["column"]
        # Blender's pixel buffer starts at bottom-left; metadata uses top-left.
        crop = alpha[(GRID - 1 - row) * cell_px : (GRID - row) * cell_px, col * cell_px : (col + 1) * cell_px]
        ys, xs = np.where(crop > 0.01)
        if not len(xs):
            raise RuntimeError(f"Atlas cell ({row}, {col}) rendered empty")
        left = int(xs.min())
        right = int(xs.max())
        top = int(cell_px - 1 - ys.max())
        bottom = int(cell_px - 1 - ys.min())
        padding = {"left": left, "top": top, "right": cell_px - 1 - right, "bottom": cell_px - 1 - bottom}
        if min(padding.values()) < 16:
            raise RuntimeError(f"Atlas cell ({row}, {col}) has less than 16 px padding: {padding}")
        record["content_bounds_px_in_cell_top_left"] = {
            "x": left,
            "y": top,
            "width": right - left + 1,
            "height": bottom - top + 1,
        }
        record["edge_padding_px"] = padding
    bpy.data.images.remove(image)


def main() -> None:
    args = cli_args()
    if args.atlas_size < 512 or args.atlas_size % GRID != 0:
        raise ValueError("--atlas-size must be at least 512 and divisible by 8")
    output_dir = resolve_output_dir(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    atlas_path = output_dir / args.atlas_name
    metadata_path = output_dir / args.metadata_name
    blend_path = output_dir / args.blend_name

    clear_scene()
    mats = build_materials()
    setup_scene(args.atlas_size, atlas_path)

    row_assets = [
        ("player_gun", build_player("gun", mats), 0.92),
        ("player_pickaxe", build_player("pickaxe", mats), 0.79),
        ("player_hands", build_player("hands", mats), 0.98),
        ("grunt_crimson", build_grunt(mats), 0.94),
        ("runner_amber", build_runner(mats), 1.02),
        ("brute_violet", build_brute(mats), 0.86),
        ("boss_red_plated", build_boss(mats), 0.73),
    ]
    cells: list[dict] = []
    direction_names = ["E", "SE", "S", "SW", "W", "NW", "N", "NE"]
    for row, (asset_name, collection, scale) in enumerate(row_assets):
        for col, direction in enumerate(direction_names):
            angle = -math.radians(col * 45.0)
            instance_collection(collection, f"{asset_name}_{direction}", col, row, angle, scale)
            cells.append(cell_record(f"{asset_name}_{direction.lower()}", row, col, args.atlas_size, direction))

    props = [
        ("forge", build_forge(mats), 0.88),
        ("ore_cyan", build_ore(mats), 0.98),
        ("barricade_horizontal", build_wall(mats, False), 0.95),
        ("barricade_vertical", build_wall(mats, True), 0.95),
        ("generator", build_generator(mats), 0.87),
        ("turret", build_turret(mats), 0.76),
        ("supply_crate", build_supply(mats), 0.95),
        ("terminal", build_terminal(mats), 0.95),
    ]
    for col, (asset_name, collection, scale) in enumerate(props):
        instance_collection(collection, asset_name, col, 7, 0.0, scale)
        cells.append(cell_record(asset_name, 7, col, args.atlas_size))

    bpy.context.scene["outpost_atlas_metadata"] = str(metadata_path)
    bpy.context.scene["outpost_atlas_layout"] = "8x8 @ 256 px when rendered at 2048"
    bpy.ops.render.render(write_still=True)
    measure_alpha_bounds(atlas_path, cells, args.atlas_size)
    write_metadata(metadata_path, atlas_path.name, blend_path.name, args.atlas_size, cells)
    bpy.ops.wm.save_as_mainfile(filepath=str(blend_path), compress=True)
    print(f"[OutpostArt] Wrote {atlas_path}")
    print(f"[OutpostArt] Wrote {metadata_path}")
    print(f"[OutpostArt] Wrote {blend_path}")


if __name__ == "__main__":
    main()
