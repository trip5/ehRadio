Import("env")
import gzip
import shutil
import time
from pathlib import Path

# Temp directory outside data folder
TEMP_BACKUP_DIR = Path(".pio/temp_www_backup")
LOCK_FILE = Path(".pio/temp_www_backup.lock")

def should_compress(source_file, gz_file):
    """Check if source file needs compression (newer than .gz or .gz doesn't exist)"""
    if not gz_file.exists():
        return True
    return source_file.stat().st_mtime > gz_file.stat().st_mtime

def compress_file(source_path):
    """Compress a single file with gzip"""
    gz_path = Path(str(source_path) + '.gz')
    
    if not should_compress(source_path, gz_path):
        return False
    
    try:
        with open(source_path, 'rb') as f_in:
            with gzip.open(gz_path, 'wb', compresslevel=9) as f_out:
                f_out.writelines(f_in)
        
        original_size = source_path.stat().st_size
        compressed_size = gz_path.stat().st_size
        ratio = (1 - compressed_size / original_size) * 100
        
        print(f"  + {source_path.name} -> {source_path.name}.gz ({original_size:,} -> {compressed_size:,} bytes, {ratio:.1f}% savings)")
        return True
    except Exception as e:
        print(f"  [error] compressing {source_path.name}: {e}")
        return False


def acquire_lock():
    """Wait for and acquire the build lock file to prevent concurrent builds"""
    max_wait = 300  # Maximum wait time in seconds
    wait_interval = 1  # Check every 1 second
    elapsed = 0
    
    while LOCK_FILE.exists():
        if elapsed == 0:
            print("\n" + "="*70)
            print("WAITING: Another build is in progress. Waiting up to 3 minutes...")
            print("\t- First, check the www files for problems")
            print(f"\t- Then, override by deleting {LOCK_FILE}")
            print("="*70)
        time.sleep(wait_interval)
        elapsed += wait_interval
        if elapsed >= max_wait:
            # ABORT the build - do NOT proceed if another build is still running
            print("\n" + "="*70)
            print(f"ERROR: Lock file timeout after {max_wait}s")
            print(f"Another build process appears stuck or is still running.")
            print(f"")
            print(f"If you're certain no other build is running, manually delete {LOCK_FILE}")
            print("="*70)
            raise SystemExit(1)  # Abort the build completely
    
    # Extra safety delay if we had to wait
    if elapsed > 0:
        print("Previous build finished, waiting 2s for safety...")
        time.sleep(2)
    
    # Create lock file
    LOCK_FILE.parent.mkdir(parents=True, exist_ok=True)
    LOCK_FILE.touch()
    print(f"\nBuild lock acquired: {LOCK_FILE}")

def compress_and_hide_originals(source, target, env):
    """Compress web files and temporarily move originals so only .gz files are in LittleFS"""
    print("\n" + "="*70)
    print("PRE-BUILD: Compressing web files for LittleFS...")
    print("="*70)
    data_dir = Path("data/www")
    if not data_dir.exists():
        print(f"Warning: {data_dir} does not exist, skipping compression")
        return
    
    # Clear and recreate temp backup directory to ensure clean state
    if TEMP_BACKUP_DIR.exists():
        shutil.rmtree(TEMP_BACKUP_DIR)
    TEMP_BACKUP_DIR.mkdir(parents=True, exist_ok=True)
    
    # Files to exclude from compression (by filename, any directory)
    exclude = ["rb_srvrs.json"]
    # Subdirectories to exclude from compression — files are kept as plain files in LittleFS
    # (avoids ESPAsyncWebServer gzip+subdirectory edge cases for small files)
    exclude_dirs = []
    
    compressed_count = 0
    skipped_count = 0
    
    print("\nGzipping files:")
    # First pass: compress all files recursively
    for file_path in sorted(data_dir.rglob("*")):
        if not file_path.is_file():
            continue
        rel = file_path.relative_to(data_dir)
        # Exclude files in excluded subdirectories
        if any(part in exclude_dirs for part in rel.parts[:-1]):
            print(f"  [skip] {rel} (excluded dir)")
            continue
        if file_path.name in exclude:
            print(f"  [skip] {rel} (excluded)")
            continue

        # Skip files that are already gzipped (avoid creating .gz.gz entries)
        if file_path.name.endswith('.gz'):
            print(f"  [skip] {file_path.relative_to(data_dir)} (already gzipped)")
            skipped_count += 1
            continue
        
        if compress_file(file_path):
            compressed_count += 1
        else:
            skipped_count += 1
    
    print("-"*70)
    print(f"Compressed: {compressed_count} files | Skipped: {skipped_count} files (already up-to-date)")
    print("="*70)
    
    # Second pass: move originals outside data directory (preserve relative subpath in backup)
    print("\nMoving original files out of data/www (only .gz and excluded files will be in LittleFS):")
    hidden_count = 0
    moved_names = []
    for file_path in sorted(data_dir.rglob("*")):
        if not file_path.is_file():
            continue
        rel = file_path.relative_to(data_dir)
        if any(part in exclude_dirs for part in rel.parts[:-1]):
            continue
        if file_path.name in exclude:
            continue
        if file_path.name.endswith('.gz'):
            continue
        
        gz_path = Path(str(file_path) + '.gz')
        if gz_path.exists():
            rel = file_path.relative_to(data_dir)
            backup_path = TEMP_BACKUP_DIR / rel
            backup_path.parent.mkdir(parents=True, exist_ok=True)
            if backup_path.exists():
                backup_path.unlink()
            shutil.move(str(file_path), str(backup_path))
            hidden_count += 1
            moved_names.append(f"-> {rel}")
    
    print(" ".join(moved_names))
    print(f"Moved {hidden_count} original files to {TEMP_BACKUP_DIR}")
    print(f"LittleFS will contain ONLY .gz files (and excluded files)")
    print("="*70 + "\n")

# Detect if we're doing a filesystem operation
import sys
any_fs_target = any(t in sys.argv for t in ["uploadfs", "buildfs", "--target"])

if any_fs_target:
    # Acquire before doing anything
    acquire_lock()
    
    # Delete cached littlefs.bin to force rebuild
    littlefs_bin = Path(env.subst("$BUILD_DIR")) / "littlefs.bin"
    if littlefs_bin.exists():
        print("\n" + "="*70)
        print("INIT: Deleting cached littlefs.bin")
        print("="*70)
        littlefs_bin.unlink()
        print("  → Deleted littlefs.bin - will rebuild with compression")
        print("="*70 + "\n")
    
    # Run compression now at init time
    compress_and_hide_originals(None, None, env)

# Note: NOT using AddPreAction here because we run compression at init time instead
# This ensures compression always runs for filesystem operations
