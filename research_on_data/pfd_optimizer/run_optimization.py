#!/usr/bin/env python3
import argparse
from pathlib import Path

from pfd_optimizer.walk_forward import optimize


def main():
    p = argparse.ArgumentParser(description="PFD hyperparameter optimizer (walk-forward)")
    p.add_argument("--config", default="config.yaml", help="Path to config.yaml")
    p.add_argument("--data", default="../aggregated_1s/aggregated_1s.csv", help="Path to aggregated_1s.csv")
    p.add_argument("--output", default="output", help="Output directory")
    args = p.parse_args()

    optimize(args.config, args.data, args.output)
    print(f"Done. Results in {Path(args.output).resolve()}")


if __name__ == "__main__":
    main()
