"""
Unified source file discovery for desktop (CMake) and Arduino (PlatformIO) builds.

Provides get_all_files_desktop and get_all_files_arduino to collect all relevant
source files with consistent include/exclude and extension filters, so other
libraries can depend on cpp_core instead of duplicating this logic.
"""

import os
from pathlib import Path
from typing import List, Optional, Set


# Default directory names that are excluded from scanning (any path segment)
DEFAULT_EXCLUDE_DIRS = {
    ".pio",      # PlatformIO build and libdeps
    ".git",
    "build",
    ".vscode",
    ".idea",
}

# Default C++ source extensions (case-insensitive)
DEFAULT_SOURCE_EXTENSIONS = [".h", ".hpp", ".cpp", ".cc", ".cxx"]


def _normalize_extensions(extensions: Optional[List[str]]) -> Set[str]:
    """Return a set of lowercase extensions, each starting with '.'."""
    if not extensions:
        return {e.lower() for e in DEFAULT_SOURCE_EXTENSIONS}
    out = set()
    for e in extensions:
        e = str(e).lower()
        if not e.startswith("."):
            e = "." + e
        out.add(e)
    return out


def _path_has_excluded_part(path: Path, exclude_dirs: Set[str]) -> bool:
    """True if any path segment is in exclude_dirs."""
    for part in path.parts:
        if part in exclude_dirs:
            return True
    return False


def _path_under_any(path: Path, exclude_paths: Set[Path]) -> bool:
    """True if path is under any of the given absolute exclude paths."""
    try:
        resolved = path.resolve()
        for ex in exclude_paths:
            try:
                if resolved.is_relative_to(ex):
                    return True
            except ValueError:
                pass
    except (ValueError, OSError):
        pass
    return False


def _walk_for_files(
    root_path: Path,
    file_extensions: Set[str],
    include_folders: Optional[List[str]],
    exclude_dirs: Set[str],
    exclude_paths: Optional[Set[Path]] = None,
) -> List[str]:
    """
    Walk a directory tree and collect file paths matching extensions and filters.

    Args:
        root_path: Directory to walk.
        file_extensions: Allowed suffixes (e.g. {'.h', '.cpp'}).
        include_folders: If set, only descend into root_path/f for each f; else whole tree.
        exclude_dirs: Directory names (path segments) that skip a subtree.
        exclude_paths: If set, paths under any of these (absolute) are skipped.

    Returns:
        Sorted list of absolute file path strings.
    """
    root_path = root_path.resolve()
    if not root_path.exists() or not root_path.is_dir():
        return []

    combined_exclude_paths = set(p.resolve() for p in exclude_paths) if exclude_paths else set()

    if include_folders:
        roots_to_walk = []
        for folder in include_folders:
            sub = root_path / folder if not os.path.isabs(folder) else Path(folder)
            if sub.exists() and sub.is_dir():
                roots_to_walk.append(sub)
        if not roots_to_walk:
            roots_to_walk = [root_path]
    else:
        roots_to_walk = [root_path]

    result = []
    for start in roots_to_walk:
        for root, dirs, files in os.walk(start):
            root_path_obj = Path(root)

            if _path_has_excluded_part(root_path_obj, exclude_dirs):
                dirs.clear()
                continue
            if combined_exclude_paths and _path_under_any(root_path_obj, combined_exclude_paths):
                dirs.clear()
                continue

            for name in files:
                p = root_path_obj / name
                if p.suffix.lower() not in file_extensions:
                    continue
                try:
                    result.append(str(p.resolve()))
                except (ValueError, OSError):
                    pass

            # Don't descend into excluded directory names
            dirs[:] = [d for d in dirs if d not in exclude_dirs]

    return sorted(result)


def _discover_libraries_desktop(project_dir: str) -> List[Path]:
    """
    Discover library root directories for a desktop (CMake) build.
    Looks under project_dir/build/_deps; includes *-src dirs or dirs with include/ or src/.
    """
    project_path = Path(project_dir).resolve()
    build_deps = project_path / "build" / "_deps"
    libraries = []
    seen = set()

    if not build_deps.exists() or not build_deps.is_dir():
        return libraries

    for lib_dir in build_deps.iterdir():
        if not lib_dir.is_dir() or lib_dir.name.startswith("."):
            continue
        name = lib_dir.name
        if name.endswith("-src"):
            lib_root = lib_dir.resolve()
            if str(lib_root) not in seen:
                seen.add(str(lib_root))
                libraries.append(lib_root)
        elif (lib_dir / "include").exists() and (lib_dir / "include").is_dir():
            lib_root = lib_dir.resolve()
            if str(lib_root) not in seen:
                seen.add(str(lib_root))
                libraries.append(lib_root)
        elif (lib_dir / "src").exists() and (lib_dir / "src").is_dir():
            lib_root = lib_dir.resolve()
            if str(lib_root) not in seen:
                seen.add(str(lib_root))
                libraries.append(lib_root)

    return libraries


def _discover_libraries_arduino(project_dir: str) -> List[Path]:
    """
    Discover library root directories for an Arduino (PlatformIO) build.
    Looks under project_dir/.pio/libdeps/<env>/<lib>.
    """
    project_path = Path(project_dir).resolve()
    pio_libdeps = project_path / ".pio" / "libdeps"
    libraries = []
    seen = set()

    if not pio_libdeps.exists() or not pio_libdeps.is_dir():
        return libraries

    for env_dir in pio_libdeps.iterdir():
        if not env_dir.is_dir():
            continue
        for lib_dir in env_dir.iterdir():
            if not lib_dir.is_dir():
                continue
            lib_root = lib_dir.resolve()
            if str(lib_root) not in seen:
                seen.add(str(lib_root))
                libraries.append(lib_root)

    return libraries


def get_all_files_desktop(
    project_dir: str,
    file_extensions: Optional[List[str]] = None,
    include_folders: Optional[List[str]] = None,
    exclude_folders: Optional[List[str]] = None,
    exclude_dirs: Optional[List[str]] = None,
    include_libraries: bool = True,
) -> List[str]:
    """
    Get all source files for a desktop (CMake) build: project + libraries from build/_deps.

    Args:
        project_dir: Project root (e.g. from CMAKE_PROJECT_DIR).
        file_extensions: Allowed suffixes (e.g. ['.h', '.cpp']). Default: C++ extensions.
        include_folders: If set, only search these subdirs of project and each library (e.g. ['src', 'include']).
        exclude_folders: Additional subdirs to skip (relative to project_dir); merged with default excludes.
        exclude_dirs: Additional directory names to skip (path segments); merged with DEFAULT_EXCLUDE_DIRS.
        include_libraries: If True, also discover and scan libraries under build/_deps.

    Returns:
        Sorted list of absolute file path strings.
    """
    project_path = Path(project_dir).resolve()
    ext_set = _normalize_extensions(file_extensions)

    exclude_dir_set = set(DEFAULT_EXCLUDE_DIRS)
    if exclude_dirs:
        exclude_dir_set.update(d.lstrip(".") if d.startswith(".") else d for d in exclude_dirs)
    if exclude_folders:
        exclude_dir_set.update(
            Path(f).parts[0] if Path(f).parts else f for f in exclude_folders
        )

    exclude_paths = set()
    if exclude_folders:
        for f in exclude_folders:
            if os.path.isabs(f):
                exclude_paths.add(Path(f).resolve())
            else:
                exclude_paths.add((project_path / f).resolve())

    all_paths = set()

    # Project files
    project_files = _walk_for_files(
        project_path,
        ext_set,
        include_folders,
        exclude_dir_set,
        exclude_paths if exclude_paths else None,
    )
    all_paths.update(project_files)

    if include_libraries:
        for lib_root in _discover_libraries_desktop(project_dir):
            lib_files = _walk_for_files(
                lib_root,
                ext_set,
                include_folders,
                exclude_dir_set,
                None,
            )
            all_paths.update(lib_files)

    return sorted(all_paths)


def get_all_files_arduino(
    project_dir: str,
    file_extensions: Optional[List[str]] = None,
    include_folders: Optional[List[str]] = None,
    exclude_folders: Optional[List[str]] = None,
    exclude_dirs: Optional[List[str]] = None,
    include_libraries: bool = True,
) -> List[str]:
    """
    Get all source files for an Arduino (PlatformIO) build: project + libraries from .pio/libdeps.

    Args:
        project_dir: Project root (e.g. from PROJECT_DIR).
        file_extensions: Allowed suffixes (e.g. ['.h', '.cpp']). Default: C++ extensions.
        include_folders: If set, only search these subdirs of project and each library (e.g. ['src', 'include']).
        exclude_folders: Additional subdirs to skip (relative to project_dir); merged with default excludes.
        exclude_dirs: Additional directory names to skip (path segments); merged with DEFAULT_EXCLUDE_DIRS.
        include_libraries: If True, also discover and scan libraries under .pio/libdeps.

    Returns:
        Sorted list of absolute file path strings.
    """
    project_path = Path(project_dir).resolve()
    ext_set = _normalize_extensions(file_extensions)

    exclude_dir_set = set(DEFAULT_EXCLUDE_DIRS)
    if exclude_dirs:
        exclude_dir_set.update(d.lstrip(".") if d.startswith(".") else d for d in exclude_dirs)
    if exclude_folders:
        for f in exclude_folders:
            part = Path(f).parts[0] if Path(f).parts else f
            exclude_dir_set.add(part)

    exclude_paths = set()
    if exclude_folders:
        for f in exclude_folders:
            if os.path.isabs(f):
                exclude_paths.add(Path(f).resolve())
            else:
                exclude_paths.add((project_path / f).resolve())

    all_paths = set()

    # Project files (excluding .pio so we don't double-count libdeps)
    project_files = _walk_for_files(
        project_path,
        ext_set,
        include_folders,
        exclude_dir_set,
        exclude_paths if exclude_paths else None,
    )
    all_paths.update(project_files)

    if include_libraries:
        for lib_root in _discover_libraries_arduino(project_dir):
            lib_files = _walk_for_files(
                lib_root,
                ext_set,
                include_folders,
                exclude_dir_set,
                None,
            )
            all_paths.update(lib_files)

    return sorted(all_paths)


def _is_cmake_build(project_dir: Optional[str] = None) -> bool:
    """
    Heuristic: True if the current context is a CMake build, False if PlatformIO.
    Uses env vars first; if neither is set, infers from project_dir layout.
    """
    if os.environ.get("CMAKE_PROJECT_DIR"):
        return True
    if os.environ.get("PROJECT_DIR"):
        return False
    if project_dir:
        proj = Path(project_dir).resolve()
        if (proj / "build" / "_deps").exists():
            return True
        if (proj / ".pio" / "libdeps").exists():
            return False
    return True  # default to desktop/CMake when ambiguous


def get_all_files_std(
    project_dir: Optional[str] = None,
    file_extensions: Optional[List[str]] = None,
    include_folders: Optional[List[str]] = None,
    exclude_folders: Optional[List[str]] = None,
    exclude_dirs: Optional[List[str]] = None,
    include_libraries: bool = True,
) -> List[str]:
    """
    Wrapper: detect CMake vs PlatformIO and call get_all_files_desktop or get_all_files_arduino.

    project_dir is taken from the first argument, or from CMAKE_PROJECT_DIR (CMake)
    or PROJECT_DIR (PlatformIO) if not provided.

    Returns:
        Sorted list of absolute file path strings.
    """
    if not project_dir:
        project_dir = os.environ.get("CMAKE_PROJECT_DIR") or os.environ.get("PROJECT_DIR")
    if not project_dir:
        project_dir = os.getcwd()

    if _is_cmake_build(project_dir):
        return get_all_files_desktop(
            project_dir,
            file_extensions=file_extensions,
            include_folders=include_folders,
            exclude_folders=exclude_folders,
            exclude_dirs=exclude_dirs,
            include_libraries=include_libraries,
        )
    return get_all_files_arduino(
        project_dir,
        file_extensions=file_extensions,
        include_folders=include_folders,
        exclude_folders=exclude_folders,
        exclude_dirs=exclude_dirs,
        include_libraries=include_libraries,
    )


if __name__ == "__main__":
    import sys

    project_dir = sys.argv[1] if len(sys.argv) > 1 else None
    mode = (sys.argv[2] if len(sys.argv) > 2 else "std").lower()

    if mode in ("arduino", "pio", "platformio"):
        files = get_all_files_arduino(project_dir or os.getcwd())
    elif mode in ("desktop", "cmake"):
        files = get_all_files_desktop(project_dir or os.getcwd())
    else:
        files = get_all_files_std(project_dir)

    for f in files:
        print(f)
