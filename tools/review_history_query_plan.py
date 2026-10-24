"""Save reproducible SQLite VM-step evidence for the reviewed latest-N query."""
import json
from pathlib import Path
import sqlite3

db = sqlite3.connect(':memory:')
db.executescript('CREATE TABLE runtime_data(id INTEGER PRIMARY KEY, variable_name TEXT, timestamp TEXT);'
                 'CREATE INDEX idx_runtime_var_time ON runtime_data(variable_name,timestamp);')
db.executemany('INSERT INTO runtime_data VALUES(?,?,?)',
               ((i, 'point', f'{i // 3:012d}') for i in range(1, 30001)))
old = ('SELECT * FROM (SELECT * FROM runtime_data WHERE variable_name=? AND id<=? '
       'ORDER BY timestamp DESC,id DESC LIMIT ?) WHERE (timestamp,id)>(?,?) ORDER BY timestamp,id LIMIT ?')
new = ('SELECT * FROM runtime_data WHERE variable_name=? AND id<=? '
       'AND (timestamp,id)>(?,?) ORDER BY timestamp,id LIMIT ?')


def measure(optimized):
    steps = 0
    def progress():
        nonlocal steps
        steps += 100
        return 0
    db.set_progress_handler(progress, 100)
    lower = db.execute('SELECT timestamp,id FROM runtime_data WHERE variable_name=? AND id<=? '
                       'ORDER BY timestamp DESC,id DESC LIMIT 1 OFFSET ?', ('point', 30000, 29999)).fetchone() if optimized else None
    cursor = ('', 0)
    ids = []
    for _ in range(30):
        args = ('point', 30000, *cursor, 1000) if optimized else ('point', 30000, 30000, *cursor, 1000)
        rows = db.execute(new if optimized else old, args).fetchall()
        ids.extend(row[0] for row in rows)
        cursor = (rows[-1][2], rows[-1][0])
    db.set_progress_handler(None, 0)
    return ids, steps


before, old_steps = measure(False)
after, new_steps = measure(True)
assert before == after and len(after) == len(set(after)) == 30000
assert new_steps < old_steps / 10, (old_steps, new_steps)
plan = db.execute('EXPLAIN QUERY PLAN ' + new, ('point', 30000, '', 0, 1000)).fetchall()
assert not any('TEMP B-TREE' in str(row) for row in plan)
result = {'sqlite_version': sqlite3.sqlite_version, 'rows': len(after), 'pages': 30,
          'old_vm_steps_approx': old_steps, 'new_vm_steps_approx': new_steps,
          'new_query_plan': plan, 'scope': 'Synthetic SQLite, not device/NAS performance acceptance'}
output = Path(__file__).resolve().parents[1] / 'review_execution/review-2026-10-23-history-plan.json'
output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
print(json.dumps(result))
