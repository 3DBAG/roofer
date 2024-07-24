import argparse
import json
from pathlib import Path

import pandas as pd
from matplotlib import pyplot as plt

parser = argparse.ArgumentParser(
    prog='PlotTraces',
    description='Plot the tracing information of roofer')
parser.add_argument('logfile')

if __name__ == "__main__":
    args = parser.parse_args()
    path_log = Path(args.logfile).absolute()

    with path_log.open() as fo:
        log_json = json.load(fo)

    # Parse the trace messages for easy plotting
    def parse_trace_message(log):
        for record in log:
            if record["level"] == "trace":
                if record["message"][0] == "{":
                    m = json.loads(record["message"])
                    del record["message"]
                    del record["name"]
                    del record["level"]
                    record["time"] = pd.to_datetime(record["time"])
                    record.update(m)
                    yield record


    trace_df = pd.DataFrame.from_records(parse_trace_message(log_json["log"]))
    start_time = trace_df.iloc[0]["time"]
    end_time = trace_df["time"].max()
    trace_df.loc[:, "duration"] = trace_df["time"].apply(lambda t: (t - start_time).total_seconds())

    ax_counts = plt.subplot(211)
    plt.tick_params("x", labelbottom=False)
    plt.grid(visible=True, which="major", axis="x")
    ax_memory = plt.subplot(212, sharex=ax_counts)
    linewidth = 2
    colormap = {
        "crop": "#2BC5F0",
        "reconstruct": "#F08B2B",
        "serialize": "#9CF02B",
        "heap": "#CD2BF0",
        "rss": "#4F8A9B"
    }
    # The expected groups are "crop", "reconstruct", "serialize", "heap", "rss"
    for name, group_df in trace_df.groupby("name"):
        if name != "heap" and name != "rss":
            ax_counts.plot(group_df["duration"], group_df["count"], label=name, color=colormap[name], linewidth=linewidth)
        else:
            ax_memory.plot(group_df["duration"], group_df["count"], label=name, color=colormap[name], linewidth=linewidth)

    ax_counts.set_ylabel("Nr. objects produced")
    ax_counts.legend()
    ax_counts.set_title(f"Total duration {(end_time - start_time).total_seconds():.2f}s")

    ax_memory.set_xlabel(f"Duration of the complete program [s]")
    ax_memory.set_ylabel('Memory usage [b]')
    ax_memory.legend()
    plt.grid(visible=True, which="major", axis="x")
    plt.savefig(f"roofer_trace_plot.png")
    plt.close()
