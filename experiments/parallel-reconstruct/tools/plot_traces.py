from pathlib import Path
import json
from matplotlib import pyplot as plt
import pandas as pd

if __name__ == "__main__":

    dir_logs = Path(__name__).parent.parent.joinpath("tests", "logs").absolute()
    dir_plots = Path(__name__).parent.parent.joinpath("tests", "plots").absolute()

    for case in ["sequential", "sequential_coro", "parallel_reconstruct", "async"]:
        path_log_crop = dir_logs / case / "log_crop.json"
        path_log_read_pc = dir_logs / case / "log_read_pc.json"
        path_log_reconstruct = dir_logs / case / "log_reconstruct.json"
        path_log_write = dir_logs / case / "log_write.json"

        df_list = []
        start_times = []
        end_times = []
        for name, process_log in [("crop", path_log_crop), ("reconstruct", path_log_reconstruct),
                                  ("write", path_log_write)]:
            with process_log.open("r") as fo:
                process_df = pd.DataFrame.from_records((json.loads(record) for record in fo))
                process_df["time"] = pd.to_datetime(process_df["time"])
                process_df.rename(columns={"name": "process"}, inplace=True)
                start_times.append(process_df.iloc[0]["time"])
                end_times.append(process_df["time"].max())
                df_list.append((name, process_df))
        start_time = min(start_times)

        plt.figure()
        for name, process_df in df_list:
                process_df.loc[:, "duration"] = process_df["time"].apply(lambda t: (t - start_time).total_seconds())
                plt.plot(process_df["duration"], process_df["count"], label=name)

        # plt.xticks(list(plt.xticks()[0]) + [(max(end_times) - start_time).total_seconds(),])
        plt.xlabel(f"Duration of the complete program [s]")
        plt.ylabel("Nr. objects produced")
        plt.title(f"Total duration {(max(end_times) - start_time).total_seconds():.2f}s")
        plt.suptitle(case)
        plt.legend()
        plt.savefig(dir_plots / case / f"{case}.png")
        plt.close()
