import os
import urllib.request
import zipfile
import shutil

# Configuration
SQLITE_YEAR = "2024"
SQLITE_VERSION = "3450100" # SQLite 3.45.1
SQLITE_URL = f"https://www.sqlite.org/{SQLITE_YEAR}/sqlite-amalgamation-{SQLITE_VERSION}.zip"
LIB_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "lib", "sqlite3")

def setup_sqlite():
    print(f"Checking SQLite dependency in {LIB_DIR}...")
    
    # Check if files already exist
    if os.path.exists(os.path.join(LIB_DIR, "sqlite3.c")) and \
       os.path.exists(os.path.join(LIB_DIR, "sqlite3.h")):
        print("SQLite sources already present.")
        return

    # Create directory
    if not os.path.exists(LIB_DIR):
        os.makedirs(LIB_DIR)

    zip_path = os.path.join(LIB_DIR, "sqlite.zip")
    
    print(f"Downloading SQLite from {SQLITE_URL}...")
    try:
        urllib.request.urlretrieve(SQLITE_URL, zip_path)
    except Exception as e:
        print(f"Error downloading SQLite: {e}")
        return

    print("Extracting...")
    try:
        with zipfile.ZipFile(zip_path, 'r') as zip_ref:
            # The zip contains a folder like sqlite-amalgamation-3450100/
            # We need to extract the files from inside that folder to LIB_DIR
            
            # Find the root folder name in the zip
            root_folder = zip_ref.namelist()[0].split('/')[0]
            
            # Extract specific files
            for file in ["sqlite3.c", "sqlite3.h", "sqlite3ext.h", "shell.c"]:
                src = f"{root_folder}/{file}"
                # shell.c might not be in amalgamation zip usually? 
                # Actually standard amalgamation has sqlite3.c, sqlite3.h, sqlite3ext.h, shell.c
                
                try:
                    # Read from zip and write to destination
                    source = zip_ref.read(src)
                    with open(os.path.join(LIB_DIR, file), 'wb') as f:
                        f.write(source)
                    print(f"Extracted {file}")
                except KeyError:
                    print(f"Warning: {file} not found in zip.")

    except Exception as e:
        print(f"Error extracting SQLite: {e}")
    finally:
        # Cleanup
        if os.path.exists(zip_path):
            os.remove(zip_path)
            
    print("SQLite setup complete.")

if __name__ == "__main__":
    setup_sqlite()
