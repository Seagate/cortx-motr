import sqlite3
import argparse

parser = argparse.ArgumentParser(description='Analyse m0trace')
parser.add_argument('--db', help = 'database file')
parser.add_argument('--trace_txt', help = 'processed trace file')
parser.add_argument('--probe_type', help = 'probe type, for ex: ha')
args = parser.parse_args()

print(args.trace_txt)
print(args.db)
trace_file = args.trace_txt
db_connect = sqlite3.connect(args.db)
db = db_connect.cursor()
db.execute("DROP TABLE IF EXISTS ha")

db.execute("CREATE TABLE ha(Time REAL, Pid INTEGER, Fid REAL, state REAL)")

matrix = []
delim=" "
with open(trace_file) as f:
    lines = f.readlines()
    for line in lines:
        list = line.split()
        #import pdb; pdb.set_trace()
        ts = list[0]
        pid = list[1]
        fid = list[2]
        state = list[3]
        print(f'ts:{ts} fid:{fid} state:{state}')
        db.execute("INSERT INTO ha (Time, Pid, Fid, state) VALUES (?, ?, ?, ?)", (ts, pid, fid, state ))

db_connect.commit()
db_connect.close()
