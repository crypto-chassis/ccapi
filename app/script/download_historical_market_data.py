#!/usr/bin/env python3
import argparse
import gzip
import os
import pathlib
import time
from datetime import datetime, timedelta

import requests
from requests.adapters import HTTPAdapter
from urllib3.util.retry import Retry


class TimeoutHTTPAdapter(HTTPAdapter):
    def __init__(self, *args, **kwargs):
        self.timeout = 5
        if "timeout" in kwargs:
            self.timeout = kwargs["timeout"]
            del kwargs["timeout"]
        super().__init__(*args, **kwargs)

    def send(self, request, **kwargs):
        timeout = kwargs.get("timeout")
        if timeout is None:
            kwargs["timeout"] = self.timeout
        return super().send(request, **kwargs)


if __name__ == "__main__":
    argumentParser = argparse.ArgumentParser()

    argumentParser.add_argument("--exchange", required=True, type=str, help="The exchange, e.g. coinbase.")
    argumentParser.add_argument("--base-asset", required=True, type=str, help="The base asset, e.g. btc.")
    argumentParser.add_argument("--quote-asset", required=True, type=str, help="The quote asset, e.g. usd.")
    argumentParser.add_argument("--start-date", required=True, type=str, help="The start date, e.g. 2021-08-22.")
    argumentParser.add_argument(
        "--end-date",
        required=True,
        type=str,
        help="The end date, e.g. 2021-08-23. Exclusive.",
    )
    argumentParser.add_argument(
        "--historical-market-data-directory",
        required=True,
        type=str,
        help="The directory in which historical market data files are saved.",
    )
    argumentParser.add_argument(
        "--historical-market-data-file-prefix",
        required=False,
        type=str,
        default="",
        help="This value specifies the name prefix of the files in which historical market data are saved.",
    )
    argumentParser.add_argument(
        "--historical-market-data-file-suffix",
        required=False,
        type=str,
        default="",
        help="This value specifies the name suffix of the files in which historical market data are saved.",
    )
    argumentParser.add_argument(
        "--depth",
        required=False,
        type=int,
        default=1,
        choices=[1, 10],
        help="The depth of market depth data.",
    )

    args = argumentParser.parse_args()

    exchange = args.exchange
    baseAsset = args.base_asset.lower()
    quoteAsset = args.quote_asset.lower()
    startDate = datetime.strptime(args.start_date, "%Y-%m-%d").date()
    endDate = datetime.strptime(args.end_date, "%Y-%m-%d").date()
    currentDate = startDate
    historicalMarketDataDirectory = args.historical_market_data_directory
    historicalMarketDataFilePrefix = args.historical_market_data_file_prefix
    historicalMarketDataFileSuffix = args.historical_market_data_file_suffix
    depth = args.depth
    pathlib.Path(historicalMarketDataDirectory).mkdir(parents=True, exist_ok=True)
    urlBase = "https://api.cryptochassis.com/v1"
    session = requests.Session()
    retries = Retry(total=10, backoff_factor=1, status_forcelist=[429, 500, 502, 503, 504])
    session.mount("https://", TimeoutHTTPAdapter(max_retries=retries))
    tradeCsvHeader = "time_seconds,price,size,is_buyer_maker\n"
    while currentDate < endDate:
        for dataType in ["market-depth", "trade"]:
            fileName = f"{historicalMarketDataFilePrefix}{exchange}__{baseAsset}-{quoteAsset}__{currentDate.isoformat()}__{dataType}{historicalMarketDataFileSuffix}"
            fileNameWithDir = f"{historicalMarketDataDirectory}/{fileName}"
            tmpFileNameWithDir = f"{historicalMarketDataDirectory}/tmp__{fileName}"
            if not pathlib.Path(f"{fileNameWithDir}.csv").is_file():
                print(f"Start download data for {dataType}, {exchange}, {baseAsset}-{quoteAsset}, {currentDate.isoformat()}.")
                requestUrl = f"{urlBase}/{dataType}/{exchange}/{baseAsset}-{quoteAsset}?startTime={currentDate.isoformat()}"
                if dataType == "market-depth":
                    requestUrl += f"&depth={depth}"
                urls = session.get(requestUrl).json()["urls"]
                time.sleep(0.1)
                if not urls:
                    print(f"Data cannot be found on server. Skip download.")
                    continue
                fileUrl = urls[0]["url"]
                with session.get(fileUrl, stream=True) as r:
                    with open(f"{fileNameWithDir}.csv.gz", "wb") as f:
                        for chunk in r.iter_content(chunk_size=1024):
                            f.write(chunk)
                with open(f"{tmpFileNameWithDir}.csv", "wb+") as fOut:
                    with gzip.open(f"{fileNameWithDir}.csv.gz", "rb") as fIn:
                        fOut.writelines(fIn)
                os.remove(f"{fileNameWithDir}.csv.gz")

                if dataType == "market-depth":
                    os.rename(f"{tmpFileNameWithDir}.csv", f"{fileNameWithDir}.csv")
                else:
                    with open(f"{tmpFileNameWithDir}.csv") as fIn:
                        firstLine = fIn.readline()
                    if firstLine == tradeCsvHeader:
                        os.rename(f"{tmpFileNameWithDir}.csv", f"{fileNameWithDir}.csv")
                    else:
                        with open(f"{fileNameWithDir}.csv", "w") as fOut:
                            fOut.write(tradeCsvHeader)
                            with open(f"{tmpFileNameWithDir}.csv") as fIn:
                                next(fIn)
                                for line in fIn:
                                    splitted = tuple(line.split(","))
                                    fOut.write(
                                        ",".join((splitted[0] + "." + splitted[1].zfill(9).rstrip("0") if splitted[1] != "0" else splitted[0],) + splitted[2:5])
                                    )
                        os.remove(f"{tmpFileNameWithDir}.csv")
        currentDate += timedelta(days=1)
