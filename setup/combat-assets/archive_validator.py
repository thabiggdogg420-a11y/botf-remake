"""Archive validator and safe extraction utility for ship assets.

Provides cryptographic verification (SHA-256), structural archive validation,
and zip-slip protection against directory traversal attacks during extraction.
"""
from pathlib import Path
import hashlib
import zipfile
from typing import Dict, Any, List, Optional


def is_safe_entry_name(name: str) -> bool:
    """Validate entry name against zip-slip and path traversal attacks."""
    if not name or "\0" in name:
        return False
    norm = name.replace("\\", "/")
    # Disallow absolute paths
    if norm.startswith("/"):
        return False
    # Disallow drive letters (e.g. C:)
    if len(norm) > 1 and norm[1] == ":":
        return False
    # Disallow parent traversal segments
    parts = norm.split("/")
    if ".." in parts:
        return False
    return True


class SafeArchiveValidator:
    """Validates and extracts archive files safely without risk of path traversal."""

    def __init__(self, archive_path: Path):
        self.archive_path = Path(archive_path).resolve()
        if not self.archive_path.is_file():
            raise FileNotFoundError(f"Archive file does not exist: {self.archive_path}")

    def compute_sha256(self) -> str:
        """Calculate the SHA-256 checksum of the archive."""
        h = hashlib.sha256()
        with open(self.archive_path, "rb") as f:
            while chunk := f.read(65536):
                h.update(chunk)
        return h.hexdigest()

    def inspect(self) -> Dict[str, Any]:
        """Inspect archive contents and return validation summary."""
        sha256 = self.compute_sha256()
        if not zipfile.is_zipfile(self.archive_path):
            return {
                "valid": False,
                "error": "Not a valid ZIP archive",
                "sha256": sha256,
                "size_bytes": self.archive_path.stat().st_size,
                "files": []
            }

        file_list: List[Dict[str, Any]] = []
        total_uncompressed = 0
        with zipfile.ZipFile(self.archive_path, "r") as zf:
            for info in zf.infolist():
                total_uncompressed += info.file_size
                safe = is_safe_entry_name(info.filename)
                file_list.append({
                    "name": info.filename,
                    "size_bytes": info.file_size,
                    "compress_size_bytes": info.compress_size,
                    "is_dir": info.is_dir(),
                    "is_safe": safe
                })

        all_safe = all(f["is_safe"] for f in file_list)
        return {
            "valid": True,
            "archive_path": str(self.archive_path),
            "sha256": sha256,
            "size_bytes": self.archive_path.stat().st_size,
            "file_count": len(file_list),
            "total_uncompressed_bytes": total_uncompressed,
            "all_entries_safe": all_safe,
            "files": file_list
        }

    def safe_extract_entry(self, entry_name: str, target_dir: Path) -> Path:
        """Safely extract a single entry into target_dir checking against traversal."""
        if not is_safe_entry_name(entry_name):
            raise ValueError(f"Unsafe entry name rejected: {entry_name}")

        target_dir = Path(target_dir).resolve()
        target_dir.mkdir(parents=True, exist_ok=True)

        with zipfile.ZipFile(self.archive_path, "r") as zf:
            matched = None
            for info in zf.infolist():
                if info.filename.replace("\\", "/") == entry_name.replace("\\", "/"):
                    matched = info
                    break
            if not matched:
                raise KeyError(f"Entry {entry_name} not found in archive {self.archive_path.name}")

            if not is_safe_entry_name(matched.filename):
                raise ValueError(f"Unsafe archive member name rejected: {matched.filename}")

            dest_path = (target_dir / matched.filename).resolve()
            if not dest_path.is_relative_to(target_dir):
                raise ValueError(f"Path traversal detected: {matched.filename} escapes {target_dir}")

            if matched.is_dir():
                dest_path.mkdir(parents=True, exist_ok=True)
                return dest_path

            dest_path.parent.mkdir(parents=True, exist_ok=True)
            with zf.open(matched, "r") as src, open(dest_path, "wb") as dst:
                while chunk := src.read(65536):
                    dst.write(chunk)
            return dest_path

    def safe_extract_filtered(self, target_dir: Path, prefixes: Optional[List[str]] = None) -> List[Path]:
        """Safely extract files whose paths match any prefix in prefixes."""
        target_dir = Path(target_dir).resolve()
        target_dir.mkdir(parents=True, exist_ok=True)
        extracted: List[Path] = []

        with zipfile.ZipFile(self.archive_path, "r") as zf:
            for info in zf.infolist():
                if not is_safe_entry_name(info.filename):
                    raise ValueError(f"Unsafe archive member name rejected: {info.filename}")

                norm_name = info.filename.replace("\\", "/")
                if prefixes:
                    if not any(norm_name.startswith(p.replace("\\", "/")) for p in prefixes):
                        continue
                if info.is_dir():
                    continue

                dest_path = (target_dir / info.filename).resolve()
                if not dest_path.is_relative_to(target_dir):
                    raise ValueError(f"Path traversal detected: {info.filename} escapes {target_dir}")

                dest_path.parent.mkdir(parents=True, exist_ok=True)
                with zf.open(info, "r") as src, open(dest_path, "wb") as dst:
                    while chunk := src.read(65536):
                        dst.write(chunk)
                extracted.append(dest_path)
        return extracted


if __name__ == "__main__":
    import sys
    if len(sys.argv) > 1:
        v = SafeArchiveValidator(Path(sys.argv[1]))
        import json
        print(json.dumps(v.inspect(), indent=2))
