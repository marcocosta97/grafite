#  This file is part of Grafite <https://github.com/marcocosta97/grafite>.
#  Copyright (C) 2023 Marco Costa.
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <https://www.gnu.org/licenses/>.

import os, argparse, sys
import shutil
import numpy as np
from pathlib import Path
from datetime import datetime

# This script is used to run a bulk test on a set of datasets and workloads for the Grafite benchmark.
global path_output_base
global benchmarks_dir

incremental_test = False
use_numa = False
membind = 0
physcpubind = 16

ds_list = ["grafite", "bucketing", "surf", "rosetta", "snarf", "proteus", 'rencoder', 'rencoder_ss', 'rencoder_se']
# ds_list_with_bucketing = ds_list.copy() + ['bucketing']

ds_benchmark_executables = {}

ds_parameters = {'grafite': list(np.linspace(8, 28, 6)),  # eps
                 'surf': [1, 4, 7, 10, 13, 16],  # suffix bits
                 'snarf': list(np.linspace(8, 28, 6)),  # bpk
                 'rosetta': list(np.linspace(8, 28, 6)),  # bpk
                 'proteus': list(np.linspace(8, 28, 6)),
                 'bucketing': list(np.linspace(8, 28, 6)),
                 'rencoder': list(np.linspace(8, 28, 6)),  # bpk
                 'rencoder_ss': list(np.linspace(8, 28, 6)),  # bpk
                 'rencoder_se': list(np.linspace(8, 28, 6))}  # bpk

ds_parameters_small_universe = {'grafite': list(np.linspace(7, 12, 6)),  # eps
                                'surf': [0, 1, 2, 3, 4, 5],  # suffix bits
                                'snarf': list(np.linspace(7, 12, 6)),  # bpk
                                'rosetta': list(np.linspace(7, 12, 6)),  # bpk
                                'proteus': list(np.linspace(7, 12, 6)),
                                'bucketing': list(np.linspace(7, 12, 6)),
                                'rencoder': list(np.linspace(7, 12, 6)),  # bpk
                                'rencoder_ss': list(np.linspace(7, 12, 6)),  # bpk
                                'rencoder_se': list(np.linspace(7, 12, 6))}  # bpk


# Format of the test directories for bulk testing
# [test] ---> [{dataset name}] ---> keys.bin
#                              |
#                              ---> [{range_size (as power of 2)}_{name (optional)}]
# e.g.
# [test] ---> [uniform] ---> keys.bin
#       |               |
#       |               ---> [10_uniform] ---> left, right
#       |               |
#       |               ---> [16_correlated] ---> left, right
#       |
#        ---> [osm] ---> ...
#
# Format of the output
# [out/{datetime}] -->  [{name}] --> [{range_size (as power of 2)}_{name}] --> {ds_name}.csv
#
# e.g.
# [out/2023-01-01 09:00:00] --> [uniform] --> [10_uniform] --> surf.csv, rosetta.csv, ...
#                           |
#                           --> [osm] --> ...
#

def execute_test(ds, keys_path, data, path_csv):
    print(keys_path)

    if keys_path and 'fb' in str(keys_path):
        param = ds_parameters_small_universe[ds]
    else:
        param = ds_parameters[ds]

    for arg in param:
        if 'rosetta' not in ds and use_numa:
            command = f'numactl --membind={membind} --physcpubind={physcpubind} {ds_benchmark_executables[ds]} {arg} --keys {keys_path} --workload {data["left"]} {data["right"]} --csv {path_csv}'
        else:
            command = f'{ds_benchmark_executables[ds]} {arg} --keys {keys_path} --workload {data["left"]} {data["right"]} --csv {path_csv}'

        print('{:^24s}'.format(f"[ starting \"{command}\"]"))
        stream = os.popen(command)
        print(stream.read())
        print('{:^24s}'.format("[ command finished ]"))
    print('{:^24s}'.format(f"[ --- {ds} finished --- ]"))
    pass


def bulk_execute(path_test_base, incremental_test=False):
    test_to_execute = parse_tests(path_test_base)
    path_old_test_base = None
    if incremental_test:
        sorted_dirs = sorted(os.listdir(path_output_base.parent), reverse=True)
        if len(sorted_dirs) < 2:
            print('cannot find a directory for incremental testing, moving to base testing')
            incremental_test = False
        else:
            path_old_test_base = Path(sorted_dirs[1])
            print(path_old_test_base)

    for dataset_name in test_to_execute:
        path_dataset = Path(f'{path_output_base}/{dataset_name}')
        path_dataset.mkdir()
        dataset = test_to_execute[dataset_name]
        keys_path = dataset['keys_path']

        for test in dataset['workloads']:
            work_path = Path(f'{path_dataset}/{test["path"].name}')
            work_path.mkdir()
            for ds in ds_list:
                path_curr_csv = Path(f'{work_path}/{ds}.csv')
                # If incremental testing is on, it looks up if the test has already been executed in the
                # last test folder and in case copies on the current output folder
                if incremental_test:
                    path_old_test_folder = str(work_path).replace(
                        str(path_output_base.name), str(path_old_test_base))
                    path_old_csv = Path(f'{path_old_test_folder}/{ds}.csv')
                    if path_old_csv.exists() and path_old_csv.is_file():
                        shutil.copyfile(path_old_csv, path_curr_csv)
                        print(
                            f'[+] {path_curr_csv} copied from {path_old_csv}')
                        continue

                # execute normal test
                with open(path_curr_csv, 'w') as document:
                    execute_test(ds, keys_path, test, path_curr_csv)
                    pass
                print(f'[+] {path_curr_csv} executed and saved')
    pass


def parse_tests(test_dir_path):
    test_dir = Path(test_dir_path)
    if not test_dir.is_dir():
        raise FileNotFoundError(f"{test_dir} does not exist")

    datasets = {}
    for subdir in test_dir.iterdir():
        if subdir.name.startswith(".") or subdir.name.endswith('.sh'):
            continue
        if not subdir.is_dir() and not subdir.name.endswith('.sh'):
            raise FileNotFoundError(
                f"error parsing the datasets, {subdir} is not a directory")

        datasets[subdir.name] = {'path': subdir, 'workloads': []}
        curr_data = datasets[subdir.name]
        for w in subdir.iterdir():
            if w.name == 'keys':
                curr_data['keys_path'] = Path(w)
            elif w.is_dir():
                work_param = w.name.split('_', 2)

                if Path(f'{w}/point').exists():
                    left = right = Path(f'{w}/point')
                else:
                    left = Path(f'{w}/left')
                    right = Path(f'{w}/right')
                if not left.is_file() or not right.is_file():
                    raise FileNotFoundError(
                        f"error parsing the datasets, workloads not found in {w}")
                curr_data['workloads'].append({'range_size': work_param[0],
                                               'path': w, 'name': w, 'left': left, 'right': right})
        if 'keys_path' not in curr_data:
            raise FileNotFoundError(
                'error, cannot find the keys file in %s' % w)

    return datasets


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        prog='BenchFiltersScript')

    parser.add_argument('-i', '--incremental', action='store_true', default=False,
                        help='if set, the script will look for the last test folder and copy the csv files in the current test folder')
    parser.add_argument('--numa', action='store_true', default=False, help='if set, the script will use numa')
    parser.add_argument('--membind', default=0, help='the numa node to use (if numa is set)')
    parser.add_argument('--physcpubind', default=16, help='the cpu to use (if numa is set)')
    parser.add_argument('--test_name', default='fpr', help='the name of the test to execute')
    parser.add_argument('test_dir', type=Path, help='the directory containing the tests')
    parser.add_argument('grafite_dir', type=Path, help='the directory containing the grafite build')

    args = parser.parse_args()
    inc = args.incremental
    test_dir = args.test_dir
    membind = args.membind
    physcpubind = args.physcpubind
    use_numa = args.numa
    test_name = args.test_name

    if not test_dir.exists():
        raise FileNotFoundError(
            'error, the test dir does not exists')

    if not args.grafite_dir.exists():
        raise FileNotFoundError(
            'error, the grafite dir does not exists')

    if test_name == 'fpr' or test_name == 'fpr_real':
        # ds_list = ds_list_with_bucketing
        pass
    elif test_name == 'lemma':
        ds_list = ['grafite']
        ds_parameters['grafite'] = [20]  # 20 bpk (L = 2^8, eps = 0.001)
    elif test_name == 'corr':
        ds_parameters = {'grafite': [20],
                         'bucketing': [20], # eps
                         'surf': [10],  # suffix bits
                         'snarf': [20],  # bpk
                         'rosetta': [20],  # bpk
                         'proteus': [20],
                         'rencoder': [20],  # bpk
                         'rencoder_ss': [20],  # bpk
                         'rencoder_se': [20]}  # bpk

    benchmarks_dir = Path(f'{args.grafite_dir}/bench/')

    for d in ds_list:
        p = Path(f'{benchmarks_dir}/bench_{d}')
        if not p.exists():
            raise FileNotFoundError(f'error, {d} benchmark not found in {benchmarks_dir}')
        ds_benchmark_executables[d] = p

    curr_time = datetime.now().strftime("%Y-%m-%d.%H:%M:%S")
    path_output_base = Path(f'./{test_name}_test/{curr_time}')
    path_output_base.mkdir(parents=True, exist_ok=True)

    try:
        bulk_execute(test_dir, incremental_test=inc)
    except Exception as e:
        print(f'received exception: {str(e)}, removing output dir and closing')
        shutil.rmtree(path_output_base)
        sys.exit(1)
    pass
