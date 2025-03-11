#!/bin/bash

# Check if the correct number of arguments is provided
if [ "$#" -ne 4 ]; then
  echo "Usage: $0 <zip1> <zip2> <output_zip> <xcfwname>"
  exit 1
fi

zip1="$1"
zip2="$2"
output_zip="$3"

# Create temporary directories for extraction
temp_dir1=$(mktemp -d)
temp_dir2=$(mktemp -d)
temp_dir_merged=$(mktemp -d)

# Extract both zip files
unzip "$zip1" -d "$temp_dir1"
unzip "$zip2" -d "$temp_dir2"

# Merge the contents from both extracted directories into the merged directory
cp -a "$temp_dir1"/* "$temp_dir_merged"
cp -a "$temp_dir2"/* "$temp_dir_merged"

# Optionally merge Info.plist files (if needed)
# You could add your plist merging logic here or call a separate Python script
# For simplicity, assuming Info.plist is in the root of each extracted directory
plist1="$temp_dir1/$4/Info.plist"
plist2="$temp_dir2/$4/Info.plist"

# If both Info.plists exist, merge them and save to merged directory
if [ -f "$plist1" ] && [ -f "$plist2" ]; then
    # Call your plist merge logic here (can be a Python script or other commands)
    # Assuming merge_plists is a command or script to merge Info.plist files
    mkdir "$temp_dir_merged/$4"
    python3 merge_plists.py "$plist1" "$plist2" "$temp_dir_merged/$4/Info.plist"
else
    # Ensure the destination directory exists before copying
    mkdir -p "$temp_dir_merged"

    # No Info.plist found in the first, copy the one that exists in the second zip
    if [ -f "$plist1" ]; then
        cp "$plist1" "$temp_dir_merged/Info.plist"
    elif [ -f "$plist2" ]; then
        cp "$plist2" "$temp_dir_merged/Info.plist"
    fi
fi

# Create a new zip file preserving symlinks
cd "$temp_dir_merged"
zip -r -y "$output_zip" ./

# Clean up temporary directories
rm -rf "$temp_dir1" "$temp_dir2" "$temp_dir_merged"

echo "Merged zip created at: $output_zip"

