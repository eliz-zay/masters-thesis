import sys
import re
import shutil

def parse_annotations(annotation_string):
  annotations = {}
  parts = annotation_string.split(';')
  for part in parts:
    if ':' in part:
      annotation, funcs = part.split(':')
      annotations[annotation] = funcs.split(',')

  return annotations

def annotate_c_file(input_c_file_path, output_c_file_path, annotation_string):
  shutil.copy(input_c_file_path, output_c_file_path)
  
  annotations = parse_annotations(annotation_string)
  print(f'Input: {annotations}')
  
  # Read and modify file content
  with open(output_c_file_path, 'r') as file:
    lines = file.readlines()
  
  function_pattern = re.compile(r'\b([a-zA-Z_][a-zA-Z0-9_]*)\s*\([^;{]*\)\s*{')  # Matches function declarations
  
  modified_lines = []
  noinline_functions = set()
  for i, line in enumerate(lines):
    match = function_pattern.search(line)
    if match:
      function_name = match.group(1)
      annotations_to_add = []
      
      for ann, funcs in annotations.items():
        if function_name in funcs:
          # Add noinline annotation exactly once for each processed function
          if function_name not in noinline_functions:
            noinline_functions.add(function_name)
            annotations_to_add.append(f'__attribute__((noinline))\n')

          annotations_to_add.append(f'__attribute__((annotate("{ann}")))\n')

      if annotations_to_add:
        modified_lines.extend(annotations_to_add)

    modified_lines.append(line)

  with open(output_c_file_path, 'w') as file:
    file.writelines(modified_lines)
  
  print(f"Annotated file saved as: {output_c_file_path}")

if __name__ == "__main__":
  if len(sys.argv) != 4:
    print("Usage: python3 annotate.py <input C file> <output C file> 'annotation_1:f1,f2;annotation_2:f1,f4,f5'")
    sys.exit(1)
  
  input_c_file_path = sys.argv[1]
  output_c_file_path = sys.argv[2]
  annotation_string = sys.argv[3]
  annotate_c_file(input_c_file_path, output_c_file_path, annotation_string)