from pathlib import Path
import json
from matplotlib import pyplot as plt
import pandas as pd

if __name__ == "__main__":

    dir_logs = Path(__name__).parent.parent.joinpath("tests", "logs").absolute()
    dir_plots = Path(__name__).parent.parent.joinpath("tests", "plots").absolute()

    for case in ["sequential",]:
        path_log_crop = dir_logs / case / "log_crop.json"
        path_log_read_pc = dir_logs / case / "log_read_pc.json"
        path_log_reconstruct = dir_logs / case / "log_reconstruct.json"
        path_log_write = dir_logs / case / "log_write.json"

        df_list = []
        for process_log in [path_log_crop, path_log_reconstruct, path_log_write]:
            with process_log.open("r") as fo:
                process_df = pd.DataFrame.from_records((json.loads(record) for record in fo))
                process_df["time"] = pd.to_datetime(process_df["time"])
                process_df.set_index("time", inplace=True)
                df_list.append(process_df)
        all_df = pd.concat(df_list, ignore_index=False, axis="rows")[["name", "count"]].pivot(columns="name", values="count")
        all_df.reset_index(inplace=True)
        all_df.rename(columns={"name": "process"}, inplace=True)
        start_time = all_df.iloc[0]["time"]
        all_df.loc[:, "duration"] = all_df["time"].apply(lambda t: (t - start_time).total_seconds())
        all_df = all_df.reset_index(inplace=False).drop(columns=["time"], inplace=False).set_index("duration", inplace=False).drop(columns=["index"])

        all_df.plot(xlabel = "Duration [s]", ylabel = "Nr. objects produced", title=case, grid=True)
        plt.savefig(dir_plots / case / f"{case}.png")
        plt.close()
