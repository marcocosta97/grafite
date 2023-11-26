#! /bin/bash

#
# This file is part of Grafite <https://github.com/marcocosta97/grafite>.
# Copyright (C) 2023 Marco Costa.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#

if [ "$#" -ne 2 ]; then
    echo "Illegal number of parameters, usage: execute_tests.sh <grafite_path> <datasets_path>"
fi

GRAFITE_BUILD_PATH=$(realpath $1)
WORKLOADS_PATH=$(realpath $2)

ARGS="--numa --membind 0 --physcpubind 16 -i" # uncomment to run with numa
# ARGS="" # comment to run without numa

SCRIPT_DIR_PATH=$(dirname -- "$( readlink -f -- "$0"; )")

OUT_PATH=./results

mkdir -p $OUT_PATH && cd $OUT_PATH || exit 1
if ! python3 $SCRIPT_DIR_PATH/test.py $ARGS --test corr $WORKLOADS_PATH/corr_test $GRAFITE_BUILD_PATH ; then
  echo "[!!] corr_test test failed"
  exit 1
fi
echo "[!!] corr_test (figure 1, 5) test executed successfully"
if ! python3 $SCRIPT_DIR_PATH/test.py $ARGS --test lemma $WORKLOADS_PATH/lemma_test $GRAFITE_BUILD_PATH ; then
  echo "[!!] lemma_test test failed"
  exit 1
fi
echo "[!!] lemma_test (figure 2) test executed successfully"
if ! python3 $SCRIPT_DIR_PATH/test.py $ARGS --test fpr $WORKLOADS_PATH/fpr_test $GRAFITE_BUILD_PATH ; then
  echo "[!!] fpr_test test failed"
  exit 1
fi
echo "[!!] fpr_test (figure 3) test executed successfully"
if ! python3 $SCRIPT_DIR_PATH/test.py $ARGS --test fpr_real $WORKLOADS_PATH/fpr_real_test $GRAFITE_BUILD_PATH ; then
  echo "[!!] fpr_real_test test failed"
  exit 1
fi
echo "[!!] fpr_real_test (figure 4) test executed successfully"
if ! python3 $SCRIPT_DIR_PATH/test.py $ARGS --test true $WORKLOADS_PATH/true_test $GRAFITE_BUILD_PATH ; then
  echo "[!!] query_time_test test failed"
  exit 1
fi
echo "[!!] query_time_test (figure 6) test executed successfully"
if ! python3 $SCRIPT_DIR_PATH/test.py $ARGS --test constr_time $WORKLOADS_PATH/constr_time_test $GRAFITE_BUILD_PATH ; then
  echo "[!!] constr_time_test test failed"
  exit 1
fi
echo "[!!] constr_time_test (figure 7) test executed successfully"

echo "[!!] success, all tests executed"
