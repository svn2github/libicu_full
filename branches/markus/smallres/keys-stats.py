import codecs
input = codecs.open("source\\data\\out\\tmp\\x86\\ReleaseBuildLog.html", "r", "utf-16")
# import sys
# input = sys.stdin

import re
_KEY_CHAR_RE = re.compile("^other-key: ([0-9a-f]{2})")
_KEY_INITIAL_RE = re.compile("^initial-key: ([0-9a-f]{2})")
_KEY_FINAL_RE = re.compile("^final-key: ([0-9a-f]{2})")

char_counts = {}
initial_counts = {}
final_counts = {}

def addItem(line, regex, item_counts):
  match = regex.search(line)
  if match:
    char = match.group(1)
    if char in item_counts:
      item_counts[char] += 1
    else:
      item_counts[char] = 1
    return True
  return False

for line in input:
  (addItem(line, _KEY_CHAR_RE, char_counts) or
   addItem(line, _KEY_INITIAL_RE, initial_counts) or
   addItem(line, _KEY_FINAL_RE, final_counts))

def printDescending(item_counts):
  count_items = [(item_counts[item], item) for item in item_counts.keys()]
  count_items.sort(reverse = True)
  for pair in count_items:
    print "'%s' = 0x%s: %4d" % (chr(int(pair[1], 16)), pair[1], pair[0])

print "*** non-alnum key chars:"
printDescending(char_counts)
print "*** non-alnum key chars as first key chars:"
printDescending(initial_counts)
print "*** non-alnum key chars as last key chars:"
printDescending(final_counts)
