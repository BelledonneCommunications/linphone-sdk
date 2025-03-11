import plistlib
import argparse
import os

def merge_plists(plist1_path, plist2_path, output_path):
    # Load the plist files
    with open(plist1_path, 'rb') as f1, open(plist2_path, 'rb') as f2:
        plist1 = plistlib.load(f1)
        plist2 = plistlib.load(f2)
    
    # Merge the AvailableLibraries arrays
    libraries1 = plist1.get("AvailableLibraries", [])
    libraries2 = plist2.get("AvailableLibraries", [])
    merged_libraries = libraries1 + libraries2
    
    # Remove duplicates (optional, based on BinaryPath + LibraryIdentifier)
    seen = set()
    unique_libraries = []
    for lib in merged_libraries:
        identifier = (lib.get("BinaryPath"), lib.get("LibraryIdentifier"))
        if identifier not in seen:
            unique_libraries.append(lib)
            seen.add(identifier)
    
    # Create the merged plist
    merged_plist = {
        "AvailableLibraries": unique_libraries,
        "CFBundlePackageType": plist1.get("CFBundlePackageType", "XFWK"),
        "XCFrameworkFormatVersion": plist1.get("XCFrameworkFormatVersion", "1.0")
    }
    
    # Write the merged plist to the output file
    with open(output_path, 'wb') as f:
        plistlib.dump(merged_plist, f)

def main():
    # Command-line argument parser
    parser = argparse.ArgumentParser(description="Merge AvailableLibraries sections of two plist files into one.")
    parser.add_argument("plist1", help="Path to the first plist file.")
    parser.add_argument("plist2", help="Path to the second plist file.")
    parser.add_argument("output", help="Path to the output merged plist file.")
    
    args = parser.parse_args()
    
    # Validate input files
    if not os.path.exists(args.plist1):
        print(f"Error: File not found: {args.plist1}")
        return
    if not os.path.exists(args.plist2):
        print(f"Error: File not found: {args.plist2}")
        return
    
    # Merge plists
    merge_plists(args.plist1, args.plist2, args.output)
    print(f"Merged plist saved to: {args.output}")

if __name__ == "__main__":
    main()

