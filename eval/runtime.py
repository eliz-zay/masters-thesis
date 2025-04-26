import os
import glob
import pandas as pd

COLUMNS = ["name", "exit_code", "stdout", "real_time", "cpu_time"]


def extract_target_mode(name):
  try:
    if not name.startswith("prog_"):
      return None, None
    target_part = name.split("prog_")[1]
    parts = target_part.rsplit("-", 1)
    if len(parts) != 2:
      return None, None
    target, mode = parts
    return target, mode
  except Exception:
    return None, None

def process_files(directory):
  data = {
    'real_time': {},
    'cpu_time': {}
  }

  pattern = os.path.join(directory, "*_return_val.csv")
  for file in glob.glob(pattern):
    # Extract target from filename
    filename = os.path.basename(file)
    if not filename.endswith("_return_val.csv"):
      continue
    target = filename.replace("_return_val.csv", "")

    df = pd.read_csv(file, header=0, names=COLUMNS, index_col=False)
    if df.empty:
      continue

    for _, row in df.iterrows():
      # Extract mode from the name field
      name = row['name']
      try:
        parts = name.split("prog_")[1].rsplit("-", 1)
        if len(parts) != 2:
          continue
        _, mode = parts
      except Exception:
        continue

      for time_type in ['real_time', 'cpu_time']:
        data[time_type][(target, mode)] = row[time_type]

  return data

def write_to_excel(data, output_file):
  writer = pd.ExcelWriter(output_file, engine='xlsxwriter')
  worksheet = writer.book.add_worksheet()

  row = 0
  worksheet.write(row, 0, "Real time")
  row += 2  # Skip a line

  real_df = pd.DataFrame(
      [(target, mode, value) for (target, mode), value in data['real_time'].items()],
      columns=['target', 'mode', 'real_time']
  ).pivot(index='target', columns='mode', values='real_time')

  for col, mode in enumerate(real_df.columns):
      worksheet.write(0, col + 1, mode)

  for row_num, (target, row_data) in enumerate(real_df.iterrows(), start=1):
      worksheet.write(row_num, 0, target)
      for col_num, value in enumerate(row_data):
          worksheet.write(row_num, col_num + 1, value)

  row += len(real_df) + 3

  worksheet.write(row, 0, "CPU time")

  cpu_df = pd.DataFrame(
      [(target, mode, value) for (target, mode), value in data['cpu_time'].items()],
      columns=['target', 'mode', 'cpu_time']
  ).pivot(index='target', columns='mode', values='cpu_time')

  for col, mode in enumerate(cpu_df.columns):
      worksheet.write(row, col + 1, mode)

  for row_num, (target, row_data) in enumerate(cpu_df.iterrows(), start=row + 1):
      worksheet.write(row_num, 0, target)
      for col_num, value in enumerate(row_data):
          worksheet.write(row_num, col_num + 1, value)

  writer.close()

def run():
  base_dir = '../OSAGE/out/roman_123/ret_compare'
  data = process_files(base_dir)
  write_to_excel(data, "roman_123.runtime.xlsx")

run()
