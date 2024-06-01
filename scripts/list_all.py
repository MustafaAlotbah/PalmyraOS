"""
This module provides various functions to write all the project in one file.
Author: Mustafa Alotbah
"""

import os
from typing import *
import pyperclip


def get_folders(path: str) -> List[str]:
    try:
        folders = [d for d in os.listdir(path) if os.path.isdir(os.path.join(path, d))]
        return folders
    except Exception as e:
        print(f"An error occurred: {e}")
        return []


def get_files(path: str) -> List[str]:
    try:
        files = [f for f in os.listdir(path) if os.path.isfile(os.path.join(path, f))]
        return files
    except Exception as e:
        print(f"An error occurred: {e}")
        return []


def read_file(path: str) -> str:
    content = []
    try:
        with open(path, 'r', encoding='utf-8') as file:
            for line in file:
                stripped_line = line.strip()
                if not stripped_line or stripped_line.startswith('//'):
                    continue
                content.append(line)
        return ''.join(content)
    except Exception as e:
        print(f"An error occurred: {e}")
        return ""


def get_hierarchy(path: str, prefix: str = '', excludes: List[str] = []) -> str:
    hierarchy = []

    # Get the list of directories and files, excluding specified substrings
    try:
        items = sorted(os.listdir(path))
    except Exception as e:
        hierarchy.append(f"Error accessing {path}: {e}")
        return '\n'.join(hierarchy)

    items = [item for item in items if not any(exclude in item for exclude in excludes)]

    for index, item in enumerate(items):
        full_path = os.path.join(path, item)
        if os.path.isdir(full_path):
            # Add directory name to the hierarchy list with a trailing '/'
            hierarchy.append(prefix + '├── ' + item + '/')
            # Recursively add the contents of the directory
            hierarchy.append(get_hierarchy(full_path, prefix + '│   ', excludes))
        else:
            # Add file name to the hierarchy list
            if index == len(items) - 1:
                hierarchy.append(prefix + '└── ' + item)
            else:
                hierarchy.append(prefix + '├── ' + item)

    return '\n'.join(hierarchy)


def get_files_recursive(path: str, excludes: List[str] = []) -> List[str]:
    files_list = []

    for root, dirs, files in os.walk(path):
        dirs[:] = [d for d in dirs if not any(exclude in d for exclude in excludes)]
        for file in files:
            if not any(exclude in file for exclude in excludes):
                full_path = os.path.join(root, file)
                relative_path = os.path.relpath(full_path, path)
                files_list.append(relative_path)

    return files_list


def get_folder_content(path: str, excludes: Optional[List[str]] = None) -> str:
    result = os.path.basename(path) + "/\n"
    result += get_hierarchy(path, excludes=excludes)
    result += "\n" * 5

    for file in get_files_recursive(path, excludes=excludes):
        result += "// File: " + os.path.basename(path) + "\\" + file
        result += "\n" * 2
        result += read_file(os.path.join(path, file))
        result += "\n" * 5

    return result


if __name__ == "__main__":
    result = get_folder_content("../PalmyraOS", excludes=["bin", "cmake-build-debug"])
    pyperclip.copy(result)
