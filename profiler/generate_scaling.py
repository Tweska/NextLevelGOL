"""
Generate figures for scaling plots.
"""
import seaborn as sns
import pandas as pd
import matplotlib.pyplot as plt
import sys
import os
import glob


def load_results():
    rows = []

    print(threads)

    # Load results in variables.
    for t in threads:
        fs = glob.glob(f"results/{results_folder}/{t}_*")
        v = results_folder[9:12]

        for run_fp in fs:
            with open(run_fp, "r") as fp:
                lines = fp.readlines()

                # Skip empty.
                if len(lines) == 0:
                    continue

                rows.append({"nthreads": int(t), "type": "init", "value": float(lines[1][12:17])})
                rows.append({"nthreads": int(t), "type": "wrap", "value": float(lines[2][12:17])})
                rows.append({"nthreads": int(t), "type": "step", "value": float(lines[3][12:17])})
                rows.append({"nthreads": int(t), "type": "swap", "value": float(lines[4][12:17])})
                rows.append({"nthreads": int(t), "type": "gif", "value": float(lines[5][12:17])})
                rows.append({"nthreads": int(t), "type": "final", "value": float(lines[6][12:17])})

                # For pthreads code, we take the actual time as total, which makes the throughput 1 row lower.
                # Including version 7.0, because it is special (and does have latency hiding without pthreads).
                if int(v[0]) >= 6:
                    rows.append({"nthreads": int(t), "type": "total", "value": float(lines[9][11:16])})
                    rows.append({"nthreads": int(t), "type": "throughput", "value": float(lines[11][12:18])})
                else:
                    rows.append({"nthreads": int(t), "type": "total", "value": float(lines[8][11:16])})
                    rows.append({"nthreads": int(t), "type": "throughput", "value": float(lines[10][12:18])})

    values = pd.DataFrame(rows)

    return values


def gen_scaling_plot():
    # Fetch DatFrame with measured values.
    df = load_results()

    print(df)

    # Create DataFrame with mean values and normalize.
    df_mean = df.pivot_table(index="nthreads",
                             columns="type",
                             values="value",
                             aggfunc="mean")

    cols = ["throughput", "total", "final", "gif", "swap", "step", "wrap", "init"]
    df_mean = df_mean[cols]

    # Create DataFrame for the error bars (std).
    df_std = df.pivot_table(index="nthreads",
                            columns="type",
                            values="value",
                            aggfunc="std")

    # Plot means.
    sns.set(style="white")
    df_mean[cols[2:]].plot(kind="bar", stacked=True, figsize=(9, 6), rot=0,
                           yerr=df_std[["step", "gif", "final"]])

    # Add info to plot.
    plt.title("Time spend per number of threads", fontsize=16)
    plt.xlabel("Number of threads", labelpad=0)
    plt.ylabel("Time spend (s)")
    plt.xticks(rotation=0)
    ax = plt.gca()
    ax.tick_params(axis="both", which="major", pad=0)
    ax.legend(loc="center left", bbox_to_anchor=(1, 0.5))
    plt.tight_layout()

    # Save and show plot.
    plt.savefig(f"figures/{results_folder}_{'-'.join(threads)}.png")
    plt.show()


if __name__ == "__main__":
    # Get results folder and versions from command line.
    args = sys.argv
    results_folder = args[1]
    threads = args[2:]

    # Check for existence of results folder.
    if not os.path.isdir(f"results/{results_folder}"):
        print(f"Given results folder '{results_folder}' does not exist..")
        exit(1)

    # Check for existence of all version results.
    for t in threads:
        files = glob.glob(f"results/{results_folder}/{t}_*")
        if len(files) == 0:
            print(f"Results for '{t}' threads do not exist..")
            exit(1)

    # Generate normalized bar plot with the given versions.
    gen_scaling_plot()
