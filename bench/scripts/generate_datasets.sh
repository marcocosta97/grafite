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
    echo "Illegal number of parameters, usage: generate_datasets.sh <grafite_path> <real_datasets_path>"
fi

GRAFITE_PATH=$(realpath $1)
REAL_DATASETS_PATH=$(realpath $2)

OUT_PATH=./datasets
# SCRIPT_DIR_PATH=$(dirname -- "$( readlink -f -- "$0"; )")

generate_corr_test() {
  i=0
  x=0.0

  while [ $i -le 10 ]
  do
    $GRAFITE_PATH/bench/workload_gen --mixed --kdist kuniform --qdist qcorrelated --corr-degree ${x}
    mv kuniform/ kuniform_${i}/
    x=$(echo $x + 0.1 | bc)
    i=$(($i + 1))
  done
}

generate_constr_time_test() {
  i=5
  x=100000
  while [ $i -le 8 ]
  do
    $GRAFITE_PATH/bench/workload_gen --kdist kuniform --qdist quniform --range-size 5 -n ${x} -q $(echo "($x * 0.1)/1" | bc)
    mv kuniform/ kuniform_${i}/
    x=$(echo "$x * 10" | bc)
    i=$(($i + 1))
  done
}

mkdir -p $OUT_PATH/corr_test && cd $OUT_PATH/corr_test || exit 1
if ! generate_corr_test ; then
  echo "[!!] corr_test generation failed"
  exit 1
fi
echo "[!!] corr_test (figure 1, 5) dataset generated"
mkdir -p ../lemma_test && cd ../lemma_test || exit 1
if ! $GRAFITE_PATH/bench/workload_gen -n 10000000 -q 1000000 --qdist quniform --range-size $(seq 0 16) ; then
  echo "[!!] lemma_test generation failed"
  exit 1
fi
echo "[!!] lemma_test (figure 2) dataset generated"
mkdir -p ../fpr_test && cd ../fpr_test || exit 1
if ! $GRAFITE_PATH/bench/workload_gen --mixed ; then
  echo "[!!] fpr_test generation failed"
  exit 1
fi
echo "[!!] fpr_test (figure 3) dataset generated"
mkdir -p ../fpr_real_test && cd ../fpr_real_test || exit 1
echo $GRAFITE_PATH/bench/workload_gen --mixed --binary-keys $REAL_DATASETS_PATH/books_200M_uint64 $REAL_DATASETS_PATH/fb_200M_uint64 $REAL_DATASETS_PATH/osm_cellids_200M_uint64
if ! $GRAFITE_PATH/bench/workload_gen --mixed --binary-keys $REAL_DATASETS_PATH/books_200M_uint64 $REAL_DATASETS_PATH/fb_200M_uint64 $REAL_DATASETS_PATH/osm_cellids_200M_uint64 ; then
  echo "[!!] fpr_real_test generation failed"
  exit 1
fi
echo "[!!] fpr_real_test (figure 4) dataset generated"
mkdir -p ../true_test && cd ../true_test || exit 1
if ! $GRAFITE_PATH/bench/workload_gen --mixed --qdist qtrue ; then
  echo "[!!] true_test generation failed"
  exit 1
fi
echo "[!!] true_test (figure 6) dataset generated"
mkdir -p ../constr_time_test && cd ../constr_time_test || exit 1
if ! generate_constr_time_test ; then
  echo "[!!] constr_time_test generation failed"
  exit 1
fi
echo "[!!] constr_time_test (figure 7) dataset generated"
echo "[!!] success, all datasets generated"