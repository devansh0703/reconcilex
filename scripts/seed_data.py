#!/usr/bin/env python3
"""Generate sample trade data for ReconcileX."""
import csv
import random
import os

random.seed(42)

SYMBOLS = ["AAPL", "GOOG", "MSFT", "AMZN", "TSLA", "META", "NVDA", "JPM", "GS", "BAC",
           "V", "MA", "UNH", "JNJ", "WMT", "PG", "XOM", "CVX", "HD", "KO"]
VENUES = ["NYSE", "NASDAQ"]
STRATEGIES = ["stat_arb", "momentum", "mean_revert", "pairs", "market_neutral"]
ACCOUNTS = [f"ACC{i:03d}" for i in range(1, 21)]


def gen_internal_csv(path: str, n: int = 5000) -> None:
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["trade_id", "timestamp_ns", "symbol", "side", "quantity", "price", "venue", "strategy", "account", "status"])
        for i in range(n):
            tid = f"INT_{i:06d}"
            ts = 1700000000000000000 + i * 1000000
            sym = random.choice(SYMBOLS)
            side = random.choice(["Buy", "Sell"])
            qty = random.randint(10, 10000)
            price = round(random.uniform(50.0, 500.0), 2)
            venue = random.choice(VENUES)
            strat = random.choice(STRATEGIES)
            acct = random.choice(ACCOUNTS)
            w.writerow([tid, ts, sym, side, qty, price, venue, strat, acct, "active"])


def gen_confirmations_csv(path: str, n: int = 5000) -> None:
    """Generate n confirmations. Intentionally create ~5 mismatches with internal trades."""
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["confirm_id", "trade_id", "symbol", "side", "quantity", "price", "venue", "status", "fees", "timestamp_ns"])
        for i in range(n):
            cid = f"CFM_{i:06d}"
            tid = f"INT_{i:06d}"
            sym = random.choice(SYMBOLS)
            side = random.choice(["Buy", "Sell"])
            qty = random.randint(10, 10000)
            price = round(random.uniform(50.0, 500.0), 2)
            venue = random.choice(VENUES)

            # Create mismatches for trades 100-104
            if 100 <= i <= 102:
                qty += random.randint(10, 50)  # qty mismatch
            elif 103 <= i <= 104:
                price += round(random.uniform(1.0, 10.0), 2)  # price mismatch

            fees = round(random.uniform(0.5, 15.0), 2)
            w.writerow([cid, tid, sym, side, qty, price, venue, "confirmed", fees, 1700000000000000000 + i * 1000000])


def gen_fix_messages(path: str, n: int = 50) -> None:
    soh = "\x01"
    with open(path, "w") as f:
        for i in range(n):
            msg_type = random.choice(["A", "0", "D", "8", "F", "AE"])
            parts = [f"8=4.4{soh}9=100{soh}35={msg_type}{soh}49=TRC{soh}56=VENUE{soh}34={i+1}{soh}52=20260101-00:00:00.000{soh}"]
            if msg_type == "D":
                parts.append(f"11=ORD{i:06d}{soh}55=AAPL{soh}54=1{soh}38=100{soh}44=150.50{soh}59=0{soh}")
            elif msg_type == "8":
                parts.append(f"37=EX{i:06d}{soh}11=ORD{i:06d}{soh}17=EXEC{i:06d}{soh}150=2{soh}39=2{soh}55=AAPL{soh}54=1{soh}38=100{soh}44=150.50{soh}")
            elif msg_type == "F":
                parts.append(f"41=ORD{i:06d}{soh}37=EX{i:06d}{soh}55=AAPL{soh}54=1{soh}38=100{soh}")
            elif msg_type == "AE":
                parts.append(f"571=TR{i:06d}{soh}55=AAPL{soh}54=1{soh}38=100{soh}44=150.50{soh}")

            body = "".join(parts)
            cs = sum(ord(c) for c in body) % 256
            f.write(body + f"10={cs:03d}{soh}\n")


if __name__ == "__main__":
    data_dir = os.path.join(os.path.dirname(__file__), "..", "data")
    os.makedirs(data_dir, exist_ok=True)
    gen_internal_csv(os.path.join(data_dir, "sample_internal.csv"), 5000)
    gen_confirmations_csv(os.path.join(data_dir, "sample_confirmations.csv"), 5000)
    gen_fix_messages(os.path.join(data_dir, "sample_fix_messages.txt"), 50)
    print(f"Generated files in {data_dir}")
