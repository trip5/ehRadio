Import("env")
import shutil
from pathlib import Path

# Temp directory where originals were backed up
TEMP_BACKUP_DIR = Path(".pio/temp_www_backup")
LOCK_FILE = Path(".pio/temp_www_backup.lock")

def restore_and_cleanup(source, target, env):
    """Restore original files and delete all .gz files after LittleFS build"""
    print("\n" + "="*70)
    print("POST-BUILD: Restoring original files and cleaning up...")
    print("="*70)
    
    data_dir = Path("data/www")

    # Delete all .gz files from data/www recursively
    print("\nDeleting all .gz files from data/www:")
    deleted_count = 0
    deleted_names = []
    for gz_file in sorted(data_dir.rglob("*.gz")):
        gz_file.unlink()
        deleted_count += 1
        deleted_names.append(f"- {gz_file.relative_to(data_dir)}")
    
    print(" ".join(deleted_names))
    print(f"Deleted: {deleted_count} .gz files")
    
    # Restore original files from backup (preserving subdirectory structure)
    restored_count = 0
    if TEMP_BACKUP_DIR.exists():
        print("\nRestoring original files to data/www:")
        restored_names = []
        for backup_file in sorted(TEMP_BACKUP_DIR.rglob("*")):
            if not backup_file.is_file():
                continue
            rel = backup_file.relative_to(TEMP_BACKUP_DIR)
            dest_path = data_dir / rel
            dest_path.parent.mkdir(parents=True, exist_ok=True)
            shutil.move(str(backup_file), str(dest_path))
            restored_count += 1
            restored_names.append(f"<- {rel}")
        
        print(" ".join(restored_names))
        print(f"Restored: {restored_count} files")
        
        # Remove temp backup directory
        try:
            TEMP_BACKUP_DIR.rmdir()
            print(f"\nRemoved temporary backup directory: {TEMP_BACKUP_DIR}")
        except:
            pass
    else:
        print(f"No backup directory found at {TEMP_BACKUP_DIR}")
    
    # Release the build lock
    if LOCK_FILE.exists():
        LOCK_FILE.unlink()
        print(f"\nBuild lock released: {LOCK_FILE}")
    
    print("-"*70)
    print(f"data/www now contains only original source files")
    print("="*70 + "\n")

# Register the post-build action
env.AddPostAction("$BUILD_DIR/littlefs.bin", restore_and_cleanup)
