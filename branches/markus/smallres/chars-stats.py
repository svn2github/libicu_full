import re
import sys

cp_counts = {}

_CODE_POINT_RE = re.compile(": ([0-9a-f]{4})")

for line in sys.stdin:
  if line.find("-char: ") < 0: continue
  line = line.rstrip()  # Remove trailing newlines etc.
  match = _CODE_POINT_RE.search(line)
  if not match: continue
  cp = match.group(1)
  if cp in cp_counts:
    cp_counts[cp] += 1
  else:
    cp_counts[cp] = 1

count_cps = [(cp_counts[cp], cp) for cp in cp_counts.keys()]
count_cps.sort(reverse = True)

for pair in count_cps[:300]:
  print "%s: %4d" % (pair[1], pair[0])
