def merge_files(source_file_path, target_file_path):
    try:
        # Read the source file content
        with open(source_file_path, 'r') as source_file:
            source_content = source_file.read()

        # Find the content to be copied from source file
        include_index = source_content.find('#include <nlohmann/json.hpp>')
        if include_index == -1:
            raise ValueError(f'#include <nlohmann/json.hpp> not found in {source_file_path}')

        content_to_copy = source_content[include_index + len('#include <nlohmann/json.hpp>\n'):]

        # Read the target file content
        with open(target_file_path, 'r') as target_file:
            target_content = target_file.read()

        # Find the line to replace content after in the target file
        target_marker = """
// LunarECL (Minseok Kim)
// https://github.com/LunarECL/LunarLog
"""

        target_marker_index = target_content.find(target_marker)
        if target_marker_index == -1:
            raise ValueError(f'Marker not found in {target_file_path}')

        # Create the new content for the target file
        new_target_content = target_content[:target_marker_index + len(target_marker)] + content_to_copy

        # Write the new content back to the target file
        with open(target_file_path, 'w') as target_file:
            target_file.write(new_target_content)

        print("Files merged successfully.")
    except Exception as e:
        print(f"An error occurred: {e}")


# Example usage
if __name__ == "__main__":
    merge_files('../include/lunar_log.hpp', '../single_include/lunar_log.hpp')
