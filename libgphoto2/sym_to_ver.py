#!/usr/bin/env python

import sys

input = open(sys.argv[1], 'r')
output = open(sys.argv[2], 'w')

print('{ global:', file=output)
for line in input:
    line = line.strip()
    print(f'{line};', file=output)
print('local: *; };', file=output)
