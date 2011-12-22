#!/usr/bin/python
import sys;
import md5;

def generate_digest_from_string(text, length):
  m = md5.new();
  m.update(text);
  dig = m.hexdigest();
  print dig
  dig = dig[0:length-1]
  return dig


if __name__ == "__main__":
  if len(sys.argv) <3:
    print "Usage: %s origin_text length" %(sys.argv[0]);
    sys.exit(1);
  digest = generate_digest_from_string(sys.argv[1], int(sys.argv[2]));
  print "Your Digest: %s\n" % digest;
