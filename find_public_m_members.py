#!/usr/bin/env python3
import re
from pathlib import Path

def extract_public_members(filepath):
    """Extract public members with m_ prefix from a header file"""
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
        
        # Find public: sections and extract members with m_
        in_public = True  # Default is public in a struct
        in_class = False
        members = []
        
        lines = content.split('\n')
        for i, line in enumerate(lines):
            stripped = line.strip()
            
            # Check if we're entering a class/struct
            if re.match(r'^(class|struct)\s+\w+', stripped):
                in_class = True
                # In a class, default is private
                in_public = 'struct' in stripped
            
            # Track public/private
            if 'public:' in stripped:
                in_public = True
            elif 'private:' in stripped:
                in_public = False
            elif 'protected:' in stripped:
                in_public = False
            
            # Find member declarations with m_ prefix in public section
            if in_public and in_class:
                # Look for type m_name patterns (public members)
                match = re.search(r'\b(bool|int|float|double|char|void|auto|std::[a-zA-Z_:]+|ID3D\w+|RECT|Vector\w*|Matrix\w*|size_t|uint\w*|int\w*|HANDLE|HWND)\s+\*?(\s*m_[a-zA-Z_]\w*)', line)
                if match and not 'private' in line[:match.start()]:
                    var_name = match.group(2).strip()
                    members.append((var_name, filepath, i+1))
        
        return members
    except Exception as e:
        return []

# Find all header files
root_dir = "/home/runner/work/GameEngine/GameEngine"
h_files = list(Path(root_dir).rglob("*.h"))

all_members = {}
for h_file in h_files:
    members = extract_public_members(str(h_file))
    for member, filepath, line_no in members:
        if member not in all_members:
            all_members[member] = []
        all_members[member].append((filepath, line_no))

# Print results
if all_members:
    print("Found public members with m_ prefix:")
    for member in sorted(all_members.keys()):
        print(f"\n{member}:")
        for filepath, line_no in all_members[member][:3]:  # Show first 3 occurrences
            print(f"  {filepath}:{line_no}")
else:
    print("No public members with m_ prefix found (or detection failed)")
