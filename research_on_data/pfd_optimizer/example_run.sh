#!/bin/bash
set -e
python run_optimization.py --config config.yaml --data ../aggregated_1s/aggregated_1s.csv --output output
