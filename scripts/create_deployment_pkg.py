import zipfile
import os
import glob

def zip_files(output_filename):
    files_to_zip = [
        "Makefile",
        "README_LINUX.md",
        "src/**/*",
        "include/**/*",
        "lib/**/*",
        "tools/**/*",
        "scripts/monitoring/monitor_4d.py",
        "scripts/shared_mem_lib.py",
        "scripts/ffi_config.py",
        "scripts/analysis/*.py",
        "scripts/maintenance/*.py",
        "scripts/optimization/*.py"
    ]

    with zipfile.ZipFile(output_filename, 'w', zipfile.ZIP_DEFLATED) as zipf:
        for pattern in files_to_zip:
            # Use recursive glob to find files
            for file in glob.glob(pattern, recursive=True):
                if os.path.isfile(file):
                    print(f"Adding {file}...")
                    zipf.write(file)
    
    print(f"Created {output_filename}")

if __name__ == "__main__":
    zip_files("SAmultithread_4d_linux.zip")
