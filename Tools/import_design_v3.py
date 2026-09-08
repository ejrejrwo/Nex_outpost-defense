"""Import the DesignV3 art set into Unreal without touching legacy content.

Run from Unreal Editor 5.8 with::

    UnrealEditor-Cmd.exe Outpost2D.uproject -run=pythonscript -script=Tools/import_design_v3.py -unattended -nullrhi

The source files are expected in ``SourceArt/DesignV3``.  This script only
imports the three explicitly listed PNG files into ``/Game/Outpost/DesignV3``.
"""

from __future__ import annotations

import sys
import traceback
from pathlib import Path

import unreal


LOG_PREFIX = "[OutpostDesignV3Import]"
DESTINATION_DIR = "/Game/Outpost/DesignV3"
PROJECT_DIR = Path(unreal.Paths.project_dir()).resolve()
SOURCE_DIR = PROJECT_DIR / "SourceArt" / "DesignV3"

DESIGN_ASSETS = (
    ("sprites_atlas.png", "T_SpritesAtlas"),
    ("arena_floor.png", "T_ArenaFloor"),
    ("key_art.png", "T_KeyArt"),
)


def _log(message: str) -> None:
    print(f"{LOG_PREFIX} {message}")


def _fail(message: str) -> None:
    raise RuntimeError(message)


def _validate_sources() -> None:
    missing = [str(SOURCE_DIR / filename) for filename, _ in DESIGN_ASSETS if not (SOURCE_DIR / filename).is_file()]
    if missing:
        _fail("Required DesignV3 source file(s) are missing: " + ", ".join(missing))


def _ensure_destination_directory() -> None:
    if unreal.EditorAssetLibrary.does_directory_exist(DESTINATION_DIR):
        return
    if not unreal.EditorAssetLibrary.make_directory(DESTINATION_DIR):
        _fail(f"Could not create Unreal content directory {DESTINATION_DIR}")
    _log(f"Created Unreal content directory: {DESTINATION_DIR}")


def _import_file(source: Path, asset_name: str):
    destination_path = f"{DESTINATION_DIR}/{asset_name}"
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source))
    task.set_editor_property("destination_path", DESTINATION_DIR)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported_paths = [str(path) for path in task.get_editor_property("imported_object_paths")]
    if not imported_paths:
        _fail(f"Unreal did not report an imported asset for {source}")

    expected_path = destination_path
    matching_paths = [path for path in imported_paths if path == expected_path or path.startswith(f"{expected_path}.")]
    if not matching_paths:
        _fail(f"Import produced an unexpected asset path for {source}: {imported_paths}")
    if len(matching_paths) != len(imported_paths):
        _fail(f"Import produced unexpected additional asset path(s) for {source}: {imported_paths}")

    texture = unreal.load_asset(expected_path)
    if texture is None:
        _fail(f"Imported asset could not be loaded at {expected_path}")
    if not isinstance(texture, unreal.Texture2D):
        _fail(f"Imported asset is not a Texture2D at {expected_path}")
    return texture


def _configure_texture(texture) -> None:
    # Keep illustrated UI art sharp and preserve atlas transparency.
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("never_stream", True)
    # Unreal's Python enum calls the UserInterface2D (RGBA) preset TC_EDITOR_ICON.
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("filter", unreal.TextureFilter.TF_BILINEAR)
    texture.set_editor_property("compression_no_alpha", False)
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
        _fail(f"Could not save texture settings for {texture.get_path_name()}")


def main() -> None:
    _log(f"Project directory: {PROJECT_DIR}")
    _log(f"Source directory: {SOURCE_DIR}")
    _validate_sources()
    _ensure_destination_directory()

    for filename, asset_name in DESIGN_ASSETS:
        source = SOURCE_DIR / filename
        texture = _import_file(source, asset_name)
        _configure_texture(texture)
        _log(f"Imported and configured {texture.get_path_name()}")

    _log("Completed DesignV3 texture import.")


if __name__ == "__main__":
    try:
        main()
    except SystemExit:
        raise
    except Exception as error:
        _log(f"ERROR: {error}")
        traceback.print_exc()
        sys.exit(1)
