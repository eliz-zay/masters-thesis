import os
import pandas as pd
from glob import glob
from collections import defaultdict

def get_files(base_dir):
  pattern = os.path.join(base_dir, 'prog_myobf-latest-*', '*-analysis', '*-softwaremetrics', 'all.csv')
  return glob(pattern)

def group_by_target(files, base_dir):
  files_by_target = defaultdict(list)

  for file_path in files:
    parts = file_path.split(base_dir)[1].split(os.sep)
    mode = parts[1].replace('prog_myobf-latest-', '')
    target = parts[2].replace('-analysis', '')

    files_by_target[target].append((mode, file_path))

  return files_by_target

def group_by_metric(files_by_target):
  grouped_by_metric = defaultdict(lambda: defaultdict(dict))

  for target, file_info_list in files_by_target.items():
    for mode, file_path in file_info_list:
      df = pd.read_csv(file_path, header=None, names=['key', 'value'])
      df = df.dropna(subset=['key'])

      for _, row in df.iterrows():
        metric = row['key']
        value = row['value']
        grouped_by_metric[metric][target][mode] = value

  return grouped_by_metric

def merge_target_metrics(file_info_list):
  merged_df = None

  for mode, file_path in sorted(file_info_list):
    df = pd.read_csv(file_path, header=None, names=['key', mode])
    df = df.dropna(subset=['key'])

    if merged_df is None:
      merged_df = df
    else:
      merged_df = pd.merge(merged_df, df, on='key', how='outer')

  return merged_df

def save_by_target(worksheet, row_offset, target, merged_df):
  worksheet.write(row_offset, 0, f"{target}")
  row_offset += 1

  for col_idx, col_name in enumerate(merged_df.columns):
    worksheet.write(row_offset, col_idx, col_name)

  for i, (_, row) in enumerate(merged_df.iterrows()):
    for j, val in enumerate(row):
      worksheet.write(row_offset + 1 + i, j, val)

def save_by_metric(worksheet, row_offset, data_by_metric, metric):
  worksheet.write(row_offset, 0, f"{metric}")
  row_offset += 1

  rows = []
  all_modes = set()

  for target in data_by_metric[metric]:
    row = {'target': target}
    for mode, value in data_by_metric[metric][target].items():
      row[mode] = value
      all_modes.add(mode)
    rows.append(row)

  columns = ['target'] + sorted(all_modes)
  df = pd.DataFrame(rows, columns=columns)

  # Write headers
  for col_idx, col_name in enumerate(df.columns):
    worksheet.write(row_offset, col_idx, col_name)

  # Write data rows
  for i, (_, row) in enumerate(df.iterrows()):
    for j, val in enumerate(row):
      worksheet.write(row_offset + 1 + i, j, val)

  return len(df) + 3

def run():
  base_dir = '../OSAGE/out/roman_123'
  xlsx_path = 'roman_123.metrics.xlsx'

  files = get_files(base_dir)
  files_by_target = group_by_target(files, base_dir)
  data_by_metric = group_by_metric(files_by_target)
  
  writer = pd.ExcelWriter(xlsx_path, engine='xlsxwriter')
  worksheet_by_target = writer.book.add_worksheet("By target")
  worksheet_by_metric = writer.book.add_worksheet("By metric")
  writer.sheets["By target"] = worksheet_by_target
  writer.sheets["By metric"] = worksheet_by_metric

  row_offset_by_target = 0

  for target, file_info_list in files_by_target.items():
    merged_df = merge_target_metrics(file_info_list)

    save_by_target(worksheet_by_target, row_offset_by_target, target, merged_df)
    row_offset_by_target += len(merged_df) + 4

  row_offset_by_metric = 0

  for metric in sorted(data_by_metric):
    offset = save_by_metric(worksheet_by_metric, row_offset_by_metric, data_by_metric, metric)
    row_offset_by_metric += offset

  writer.close()



run()
