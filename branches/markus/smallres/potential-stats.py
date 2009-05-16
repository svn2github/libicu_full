# file,size,#strings,str.size,save,#mini,mini.save,incompressible,comp.pad,keys.save,dedup.save,key.size

import codecs
input = codecs.open("source\\data\\out\\tmp\\x86\\ReleaseBuildLog.html", "r", "utf-16")
# import sys
# input = sys.stdin

bundles_count = 0
sum_bundle_size = 0
strings_count = 0
sum_strings_size = 0
sum_saved = 0
mini_count = 0
sum_mini_saved = 0
incompressible_count = 0
sum_compressed_padding = 0
sum_keys_size = 0
sum_keys_saved = 0
sum_dedup_saved = 0

for line in input:
  if not line.startswith("potential: "): continue
  if line.find("testdata") >= 0: continue
  line = line.rstrip()  # Remove trailing newlines etc.
  index = line.find("icudt42l\\")
  line = line[index + 9:].replace("\\", "/")
  bundles_count += 1
  fields = line.split(",")
  sum_bundle_size += int(fields[1])
  strings_count += int(fields[2])
  sum_strings_size += int(fields[3])
  sum_saved += int(fields[4])
  mini_count += int(fields[5])
  sum_mini_saved += int(fields[6])
  incompressible_count += int(fields[7])
  sum_compressed_padding += int(fields[8])
  sum_keys_saved += int(fields[9])
  sum_dedup_saved += int(fields[10])
  sum_keys_size += int(fields[11])

print "number of resource bundles: %d" % bundles_count
print "  total size without headers: %d bytes" % sum_bundle_size
print "  not including savings from key suffix sharing: %d bytes" % sum_keys_saved
print "  not including savings from string de-duplication: %d bytes" % sum_dedup_saved
print "keys size: %d bytes" % sum_keys_size
print "number of strings: %d" % strings_count
print "  strings size including resource items: %d bytes" % sum_strings_size
print ("size reduction: %d (minus about %d*16=%d bytes in header)" %
       (sum_saved, bundles_count, bundles_count * 16))
print "  total padding after compressed strings: %d bytes" % sum_compressed_padding
print "number of strings that fit mini format: %d" % mini_count
print "  savings from those: %d" % sum_mini_saved
print ("%d strings are incompressible (longer in \"compressed\" " +
       "form than in original UTF-16)") % incompressible_count
